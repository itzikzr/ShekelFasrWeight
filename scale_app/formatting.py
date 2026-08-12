"""
עיצוב משקל משותף לכל המסכים.
"""


def fmt_weight(value: float, decimals: int = 3, force_sign: bool = False) -> str:
    """
    מעגל ומעצב משקל, ומונע '-0.000' — משקל שמתעגל לאפס מוצג כ-'0.000' (או '+0.000'
    אם force_sign), בלי סימן שלילי שמרמז על משקל שלילי שלא קיים בפועל.
    """
    rounded = round(value, decimals)
    if rounded == 0:
        rounded = 0.0
    if force_sign:
        return f"{rounded:+.{decimals}f}"
    return f"{rounded:.{decimals}f}"
