#pragma once

#include "core/catalog.h"
#include "core/shelf-life.h"

#include <QTest>

namespace whilefresh::core::testing {

// A small hand-written vocabulary covering parents, pantry and bulk defaults.
inline IngredientCatalog smallCatalog()
{
    const QByteArray json = R"({
      "categories": [{"code": "vegetable", "name": "蔬菜"}, {"code": "meat", "name": "肉类"},
                     {"code": "egg_dairy", "name": "蛋奶"}, {"code": "staple", "name": "主食粮油"},
                     {"code": "seasoning", "name": "调料"}],
      "items": [
        {"name": "西红柿", "category": "vegetable", "aliases": ["番茄"]},
        {"name": "圣女果", "category": "vegetable", "aliases": ["小番茄"], "parent": "西红柿"},
        {"name": "土豆", "category": "vegetable", "aliases": []},
        {"name": "猪肉", "category": "meat", "aliases": []},
        {"name": "五花肉", "category": "meat", "aliases": [], "parent": "猪肉"},
        {"name": "牛奶", "category": "egg_dairy", "aliases": []},
        {"name": "淡奶油", "category": "egg_dairy", "aliases": []},
        {"name": "鸡蛋", "category": "egg_dairy", "aliases": [], "bulk_default": true},
        {"name": "食用油", "category": "staple", "aliases": [], "pantry_default": true, "bulk_default": true},
        {"name": "盐", "category": "seasoning", "aliases": [], "pantry_default": true, "bulk_default": true},
        {"name": "白糖", "category": "seasoning", "aliases": [], "pantry_default": true, "bulk_default": true},
        {"name": "酱油", "category": "seasoning", "aliases": [], "pantry_default": true, "bulk_default": true},
        {"name": "生抽", "category": "seasoning", "aliases": [], "parent": "酱油", "pantry_default": true, "bulk_default": true},
        {"name": "豆瓣酱", "category": "seasoning", "aliases": [], "bulk_default": true},
        {"name": "葱", "category": "vegetable", "aliases": [], "pantry_default": true},
        {"name": "小葱", "category": "vegetable", "aliases": [], "parent": "葱", "pantry_default": true}
      ]})";
    QString error;
    auto catalog = IngredientCatalog::fromJson(json, &error);
    if (!catalog)
        qFatal("small catalog: %s", qUtf8Printable(error));
    return *catalog;
}

// Reference rows in the format T3 generates (app/assets/data/shelf-life.json).
inline ShelfLifeTable smallShelfLifeTable()
{
    const QByteArray json = R"({
      "猪肉": {"fridge_days": 3, "freezer_days": 120, "room_days": null, "source": "USDA FoodKeeper",
               "foodkeeper_id": 123, "basis": "DOP min"},
      "鸡蛋": {"fridge_days": 21, "freezer_days": null, "room_days": null, "source": "USDA FoodKeeper",
               "foodkeeper_id": "77", "basis": "DOP min"},
      "土豆": {"fridge_days": null, "freezer_days": null, "room_days": 21, "source": "USDA FoodKeeper",
               "foodkeeper_id": 9, "basis": "DOP min"}
    })";
    QString error;
    auto table = ShelfLifeTable::fromJson(json, &error);
    if (!table)
        qFatal("small table: %s", qUtf8Printable(error));
    return *table;
}

}
