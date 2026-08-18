"""
היסטוריית שקילות — כל השקילות שנרשמו ל-DB, עם סינון לפי מוצר/תוצאה וייצוא ל-CSV.
"""

import csv
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from . import db, rtl, theme
from .formatting import fmt_weight
from .widgets import DatePicker

VERDICT_LABELS = {"green": "ירוק — בטווח", "red": "אדום — מעל", "yellow": "צהוב — מתחת",
                  "none": "ללא מוצר"}
ALL_LABEL = "הכל"


class HistoryWindow(tk.Toplevel):
    def __init__(self, main):
        super().__init__(main.root)
        self.main = main
        self.title("היסטוריית שקילות")
        self.minsize(920, 540)
        theme.maximize_window(self)
        self.configure(background=theme.BG)

        self._current_rows = []
        # Combobox values are rendered by Tk itself (bidi-reordering the
        # displayed text on Linux), but filtering compares the selection
        # against original DB/label strings — these maps translate a
        # (possibly reordered) displayed value back to the logical one.
        # No-op dicts on Windows, where rtl.visual() is the identity.
        self._product_display_to_name = {}

        filters = ttk.Frame(self)
        filters.pack(fill="x", padx=10, pady=(10, 6))

        ttk.Label(filters, text="מוצר:").pack(side="right", padx=(0, 4))
        self.product_filter_var = tk.StringVar(value=rtl.visual(ALL_LABEL))
        self.product_filter_combo = ttk.Combobox(filters, textvariable=self.product_filter_var,
                                                  state="readonly", width=20)
        self.product_filter_combo.pack(side="right", padx=(0, 16))
        self.product_filter_combo.bind("<<ComboboxSelected>>", lambda e: self.refresh())

        ttk.Label(filters, text="תוצאה:").pack(side="right", padx=(0, 4))
        verdict_values = [ALL_LABEL] + list(VERDICT_LABELS.values())
        display_verdict_values = [rtl.visual(v) for v in verdict_values]
        self._verdict_display_to_label = dict(zip(display_verdict_values, verdict_values))
        self.verdict_filter_var = tk.StringVar(value=rtl.visual(ALL_LABEL))
        self.verdict_filter_combo = ttk.Combobox(filters, textvariable=self.verdict_filter_var,
                                                  state="readonly", width=18,
                                                  values=display_verdict_values)
        self.verdict_filter_combo.pack(side="right", padx=(0, 16))
        self.verdict_filter_combo.bind("<<ComboboxSelected>>", lambda e: self.refresh())

        ttk.Label(filters, text="מתאריך:").pack(side="right", padx=(0, 4))
        self.date_from_picker = DatePicker(filters, on_change=self.refresh)
        self.date_from_picker.pack(side="right", padx=(0, 16))

        ttk.Label(filters, text="עד תאריך:").pack(side="right", padx=(0, 4))
        self.date_to_picker = DatePicker(filters, on_change=self.refresh)
        self.date_to_picker.pack(side="right", padx=(0, 16))

        ttk.Button(filters, text="נקה תאריכים",
                  command=self._clear_date_filter).pack(side="right", padx=(0, 16))

        ttk.Button(filters, text="ייצוא ל-CSV", command=self._export_csv).pack(side="left")

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)

        cols = ("#", "זמן", "מוצר", "משקל (kg)", "מטרה", "תוצאה", "קריאות", "משך")
        widths = (55, 140, 170, 90, 90, 140, 70, 70)
        anchors = ("center", "center", "e", "e", "e", "center", "center", "center")
        self.tree = ttk.Treeview(body, columns=cols, show="headings")
        for c, w, a in zip(cols, widths, anchors):
            self.tree.heading(c, text=c)
            self.tree.column(c, width=w, anchor=a, stretch=(c == "מוצר"))
        self.tree.grid(row=0, column=0, sticky="nsew")

        sb = ttk.Scrollbar(body, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        sb.grid(row=0, column=1, sticky="ns")

        for verdict, (fg, bg) in theme.VERDICT_COLORS.items():
            self.tree.tag_configure(verdict, background=bg, foreground=fg)

        self.refresh()
        self.protocol("WM_DELETE_WINDOW", self.destroy)

    def refresh_theme(self):
        self.configure(background=theme.BG)
        for verdict, (fg, bg) in theme.VERDICT_COLORS.items():
            self.tree.tag_configure(verdict, background=bg, foreground=fg)

    def _refresh_product_filter(self):
        names = [ALL_LABEL] + [p["name"] for p in db.list_products()]
        current_display = self.product_filter_var.get()
        current_name = self._product_display_to_name.get(current_display, current_display)

        display_names = [rtl.visual(n) for n in names]
        self._product_display_to_name = dict(zip(display_names, names))
        self.product_filter_combo.config(values=display_names)
        if current_name not in names:
            self.product_filter_var.set(rtl.visual(ALL_LABEL))

    def _clear_date_filter(self):
        self.date_from_picker.clear()
        self.date_to_picker.clear()
        self.refresh()

    def _date_range_bound(self):
        """
        (since, until) בפורמט timestamp המחרוזתי של ה-DB, מ-DatePicker.get()
        (תמיד "" או "YYYY-MM-DD" תקין — נבחר מלוח שנה, לא מוקלד, אז אין צורך
        באימות פורמט). since מחצות יום ה"מתאריך", until מהשנייה האחרונה של
        יום ה"עד" (כולל את כל היום, לא רק חצות שלו). None לשניהם אם ריק.
        """
        from_text = self.date_from_picker.get()
        to_text = self.date_to_picker.get()
        since = f"{from_text}T00:00:00" if from_text else None
        until = f"{to_text}T23:59:59" if to_text else None
        return since, until

    def refresh(self):
        self._refresh_product_filter()

        product_id = None
        product_display = self.product_filter_var.get()
        product_name = self._product_display_to_name.get(product_display, product_display)
        if product_name != ALL_LABEL:
            for p in db.list_products():
                if p["name"] == product_name:
                    product_id = p["id"]
                    break

        verdict = None
        verdict_display = self.verdict_filter_var.get()
        verdict_label = self._verdict_display_to_label.get(verdict_display, verdict_display)
        if verdict_label != ALL_LABEL:
            for key, label in VERDICT_LABELS.items():
                if label == verdict_label:
                    verdict = key
                    break

        since, until = self._date_range_bound()
        rows = db.list_weighings(product_id=product_id, verdict=verdict, since=since, until=until)
        self._current_rows = rows

        self.tree.delete(*self.tree.get_children())
        for r in rows:
            target_str = fmt_weight(r["target_weight"]) if r["target_weight"] is not None else "—"
            product_str = r["product_name"] or "— ללא מוצר —"
            elapsed_str = f"{r['elapsed_seconds']:.2f}s" if r["elapsed_seconds"] is not None else "—"
            self.tree.insert("", "end", tags=(r["verdict"],),
                             values=(r["id"], r["timestamp"], product_str,
                                     fmt_weight(r["decided_weight"], force_sign=True), target_str,
                                     VERDICT_LABELS.get(r["verdict"], r["verdict"]),
                                     r["reading_count"] or 0, elapsed_str))

    def _export_csv(self):
        path = filedialog.asksaveasfilename(defaultextension=".csv",
                                            filetypes=[("CSV", "*.csv")],
                                            initialfile="weighings.csv")
        if not path:
            return
        try:
            with open(path, "w", newline="", encoding="utf-8-sig") as f:
                writer = csv.writer(f)
                writer.writerow(["id", "timestamp", "product", "target_weight",
                                 "tolerance_upper", "tolerance_lower", "decided_weight",
                                 "verdict", "reading_count", "elapsed_seconds",
                                 "min_weight", "max_weight", "basis"])
                for r in self._current_rows:
                    writer.writerow([r["id"], r["timestamp"], r["product_name"],
                                     r["target_weight"], r["tolerance_upper"],
                                     r["tolerance_lower"], r["decided_weight"], r["verdict"],
                                     r["reading_count"], r["elapsed_seconds"],
                                     r["min_weight"], r["max_weight"], r["basis"]])
        except Exception as e:
            messagebox.showerror("שגיאה", str(e))
            return
        messagebox.showinfo("ייצוא הושלם", f"נשמר אל:\n{path}")
