"""
ניהול מוצרים — שם, משקל מטרה, טולרנס עליון/תחתון. משמש לסיווג הרמזור בשקילה.
"""

import sqlite3
import tkinter as tk
from tkinter import ttk, messagebox

from . import db, rtl, theme


class ProductsWindow(tk.Toplevel):
    def __init__(self, main):
        super().__init__(main.root)
        self.main = main
        self.title("מוצרים")
        self.minsize(640, 420)
        theme.maximize_window(self)
        self.configure(background=theme.BG)

        cols = ("שם", "משקל מטרה (kg)", "טולרנס עליון", "טולרנס תחתון")
        self.tree = ttk.Treeview(self, columns=cols, show="headings")
        for c, w in zip(cols, (200, 140, 130, 130)):
            self.tree.heading(c, text=c)
            self.tree.column(c, width=w, anchor="center")
        self.tree.pack(fill="both", expand=True, padx=10, pady=(10, 6))
        self.tree.bind("<Double-1>", lambda e: self._on_edit())

        btns = ttk.Frame(self)
        btns.pack(fill="x", padx=10, pady=(0, 10))
        ttk.Button(btns, text="+ מוצר חדש", style="Accent.TButton",
                   command=self._on_add).pack(side="left", padx=4)
        ttk.Button(btns, text="ערוך", command=self._on_edit).pack(side="left", padx=4)
        ttk.Button(btns, text="מחק", command=self._on_delete).pack(side="left", padx=4)

        self._id_by_iid = {}
        self._refresh()
        self.protocol("WM_DELETE_WINDOW", self.destroy)

    def refresh_theme(self):
        self.configure(background=theme.BG)

    def _refresh(self):
        self.tree.delete(*self.tree.get_children())
        self._id_by_iid = {}
        for p in db.list_products():
            iid = self.tree.insert("", "end", values=(
                p["name"], f"{p['target_weight']:.3f}",
                f"+{p['tolerance_upper']:.3f}", f"-{p['tolerance_lower']:.3f}"))
            self._id_by_iid[iid] = p["id"]

    def _selected_id(self):
        sel = self.tree.selection()
        return self._id_by_iid.get(sel[0]) if sel else None

    def _on_add(self):
        ProductFormDialog(self, on_save=self._save_new)

    def _save_new(self, name, target, tol_up, tol_low):
        try:
            db.add_product(name, target, tol_up, tol_low)
        except sqlite3.IntegrityError:
            messagebox.showerror("שגיאה", "שם מוצר זה כבר קיים")
            return
        self._refresh()
        self.main.refresh_products()

    def _on_edit(self):
        product_id = self._selected_id()
        if product_id is None:
            messagebox.showinfo("ערוך", "בחר מוצר מהרשימה")
            return
        product = db.get_product(product_id)
        ProductFormDialog(self, product=product,
                          on_save=lambda *a: self._save_edit(product_id, *a))

    def _save_edit(self, product_id, name, target, tol_up, tol_low):
        try:
            db.update_product(product_id, name, target, tol_up, tol_low)
        except sqlite3.IntegrityError:
            messagebox.showerror("שגיאה", "שם מוצר זה כבר קיים")
            return
        self._refresh()
        self.main.refresh_products()

    def _on_delete(self):
        product_id = self._selected_id()
        if product_id is None:
            messagebox.showinfo("מחק", "בחר מוצר מהרשימה")
            return
        product = db.get_product(product_id)
        if not messagebox.askyesno("מחיקה", f"למחוק את המוצר '{product['name']}'?"):
            return
        db.delete_product(product_id)
        self._refresh()
        self.main.refresh_products()


class ProductFormDialog(tk.Toplevel):
    def __init__(self, parent, product=None, on_save=None):
        super().__init__(parent)
        self.on_save = on_save
        self.title("מוצר חדש" if product is None else "ערוך מוצר")
        self.resizable(False, False)
        self.grab_set()
        self.focus_set()
        self.configure(background=theme.BG)

        # The Entry itself isn't bidi-patched (unlike Label/Button text) since
        # it's editable and its value is read back to write the DB — see
        # rtl.py. Pre-filling it with the reordered name for correct display
        # would corrupt the stored name on save if the user clicks Save
        # without retyping, so _on_save() detects "unchanged" and substitutes
        # back the real original_name. On Windows both strings are identical
        # and this is a no-op.
        self._original_name = product["name"] if product else ""
        prefilled_display_name = rtl.visual(self._original_name)
        self._prefilled_display_name = prefilled_display_name

        pad = {"padx": 8, "pady": 8}
        ttk.Label(self, text="שם מוצר:").grid(row=0, column=0, sticky="e", **pad)
        self.name_var = tk.StringVar(value=prefilled_display_name)
        ttk.Entry(self, textvariable=self.name_var, width=26).grid(row=0, column=1, **pad)

        ttk.Label(self, text="משקל מטרה (kg):").grid(row=1, column=0, sticky="e", **pad)
        self.target_var = tk.DoubleVar(value=product["target_weight"] if product else 1.0)
        ttk.Spinbox(self, textvariable=self.target_var, from_=0.0, to=99999.0,
                    increment=0.01, width=12, format="%.3f").grid(row=1, column=1, sticky="w", **pad)

        ttk.Label(self, text="טולרנס עליון (kg):").grid(row=2, column=0, sticky="e", **pad)
        self.tol_up_var = tk.DoubleVar(value=product["tolerance_upper"] if product else 0.05)
        ttk.Spinbox(self, textvariable=self.tol_up_var, from_=0.0, to=9999.0,
                    increment=0.01, width=12, format="%.3f").grid(row=2, column=1, sticky="w", **pad)

        ttk.Label(self, text="טולרנס תחתון (kg):").grid(row=3, column=0, sticky="e", **pad)
        self.tol_low_var = tk.DoubleVar(value=product["tolerance_lower"] if product else 0.05)
        ttk.Spinbox(self, textvariable=self.tol_low_var, from_=0.0, to=9999.0,
                    increment=0.01, width=12, format="%.3f").grid(row=3, column=1, sticky="w", **pad)

        btns = ttk.Frame(self)
        btns.grid(row=4, column=0, columnspan=2, pady=(6, 10))
        ttk.Button(btns, text="שמור", style="Accent.TButton",
                   command=self._on_save).pack(side="left", padx=6)
        ttk.Button(btns, text="ביטול", command=self.destroy).pack(side="left", padx=6)

    def _on_save(self):
        name = self.name_var.get().strip()
        if name == self._prefilled_display_name.strip() and self._original_name:
            name = self._original_name
        if not name:
            messagebox.showerror("שגיאה", "יש להזין שם מוצר")
            return
        try:
            target = float(self.target_var.get())
            tol_up = float(self.tol_up_var.get())
            tol_low = float(self.tol_low_var.get())
        except (tk.TclError, ValueError):
            messagebox.showerror("שגיאה", "ערכים לא תקינים")
            return
        if self.on_save:
            self.on_save(name, target, tol_up, tol_low)
        self.destroy()
