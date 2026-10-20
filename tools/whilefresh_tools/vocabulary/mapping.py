"""Apply the ingredient map to recipes.json."""

from .schema import MAP_PATH, load_json


class MissingMapping(ValueError):
    def __init__(self, names):
        self.names = sorted(names)
        super().__init__(
            f"{len(self.names)} recipe ingredient name(s) missing from the ingredient map: "
            + ", ".join(repr(n) for n in self.names[:20])
            + (" ..." if len(self.names) > 20 else "")
        )


def load_map(path=MAP_PATH) -> dict[str, list[str]]:
    return load_json(path)["map"]


def annotate_recipes(doc: dict, mapping: dict[str, list[str]]) -> dict:
    """Add ``canonical`` to every ingredient of ``doc`` in place; tools get ``[]``."""
    missing = set()
    for recipe in doc["recipes"]:
        for ing in recipe["ingredients"]:
            if ing["kind"] == "tool":
                ing["canonical"] = []
            elif ing["name"] in mapping:
                ing["canonical"] = list(mapping[ing["name"]])
            else:
                missing.add(ing["name"])
    if missing:
        raise MissingMapping(missing)
    return doc
