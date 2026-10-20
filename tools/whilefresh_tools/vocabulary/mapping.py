"""Apply the ingredient map to recipes.json."""

from .schema import MAP_PATH, load_json, normalize_groups


class MissingMapping(ValueError):
    def __init__(self, names):
        self.names = sorted(names)
        super().__init__(
            f"{len(self.names)} recipe ingredient name(s) missing from the ingredient map: "
            + ", ".join(repr(n) for n in self.names[:20])
            + (" ..." if len(self.names) > 20 else "")
        )


def load_map(path=MAP_PATH) -> dict[str, list[list[str]]]:
    """The map with every value normalised to requirement groups."""
    return {k: normalize_groups(v) for k, v in load_json(path)["map"].items()}


def annotate_recipes(doc: dict, mapping: dict) -> dict:
    """Add ``canonical`` (requirement groups) to every ingredient of ``doc`` in place;
    tools get ``[]``. Flat string lists in ``mapping`` are accepted as shorthand."""
    missing = set()
    for recipe in doc["recipes"]:
        for ing in recipe["ingredients"]:
            if ing["kind"] == "tool":
                ing["canonical"] = []
            elif ing["name"] in mapping:
                ing["canonical"] = [list(g) for g in normalize_groups(mapping[ing["name"]])]
            else:
                missing.add(ing["name"])
    if missing:
        raise MissingMapping(missing)
    return doc
