#pragma once

#include <QString>

// Every user-visible sentence the domain produces, in one place so wording stays consistent and
// never claims food safety ("可安全食用", "已变质" are not used anywhere).
namespace whilefresh::core::texts {

inline QString freshnessFresh() { return QStringLiteral("新鲜"); }
inline QString freshnessNearExpiry() { return QStringLiteral("临期"); }
inline QString freshnessBeyond() { return QStringLiteral("超出估计期限"); }
inline QString freshnessUnknown() { return QStringLiteral("期限未知"); }
inline QString freshnessBeyondHint() { return QStringLiteral("超出估计期限，请查看后决定"); }
inline QString freshnessUnknownHint() { return QStringLiteral("还不知道能放多久，可以手动填写"); }

inline QString sourcePackage() { return QStringLiteral("包装标注"); }
inline QString sourceManual() { return QStringLiteral("你填写的"); }
inline QString sourceLocalRule() { return QStringLiteral("你的设置"); }
inline QString sourceReference() { return QStringLiteral("参考表"); }
inline QString sourceAiEstimate() { return QStringLiteral("AI 估计"); }
inline QString sourceNone() { return QStringLiteral("暂无依据"); }
inline QString estimateTag() { return QStringLiteral("估计"); }
inline QString referenceAttribution() { return QStringLiteral("参考：USDA FoodKeeper"); }

inline QString statusInStock() { return QStringLiteral("在库"); }
inline QString statusUsed() { return QStringLiteral("已用掉"); }
inline QString statusAbsent() { return QStringLiteral("家里没有"); }
inline QString statusDiscarded() { return QStringLiteral("已扔掉"); }
inline QString needsShelfLife() { return QStringLiteral("待录保质期"); }

inline QString orSeparator() { return QStringLiteral(" 或 "); }
inline QString shoppingListTitle() { return QStringLiteral("待买清单"); }

inline QString importMayBeIncomplete() { return QStringLiteral("可能有遗漏，请检查"); }
inline QString importNeedsBaseDate()
{
    return QStringLiteral("图片上有“今天生产”这类日期，但读不到图片的拍摄日期，请确认是哪天拍的");
}
inline QString importDuplicateImage() { return QStringLiteral("这张图片之前已经导入过"); }
inline QString importPossibleDuplicate() { return QStringLiteral("这次的商品和最近一次导入很像，可能是重复导入"); }
inline QString importIgnoredGroup() { return QStringLiteral("已忽略"); }
inline QString importBusy() { return QStringLiteral("正在识别上一次导入，请稍候"); }
inline QString importNothingToConfirm() { return QStringLiteral("没有选中任何商品"); }
inline QString importEmptyInput() { return QStringLiteral("没有可识别的图片或文字"); }

// Shown before anything is uploaded; the user must agree first.
inline QString uploadPrivacyNotice()
{
    return QStringLiteral("将把所选图片或文字发送到 DeepSeek 识别商品。截图里可能有收货地址、电话、"
                          "姓名等个人信息，建议先裁掉这些部分再上传。识别结果只保存在本机。");
}
inline QString uploadPrivacyAgree() { return QStringLiteral("同意并识别"); }
inline QString demoLabel() { return QStringLiteral("演示"); }

inline QString suggestionMarkPantryHave(const QString &name)
{
    return QStringLiteral("这次买到了「%1」，标记为家里有吗？").arg(name);
}
inline QString suggestionAddToPantry(const QString &name)
{
    return QStringLiteral("把「%1」加入常备，以后默认算家里有吗？").arg(name);
}
inline QString suggestionAddToShopping(const QString &name)
{
    return QStringLiteral("「%1」家里没有了，加入待买吗？").arg(name);
}
inline QString ruleQuestionShelfLife(const QString &name, const QString &storage, int days)
{
    return QStringLiteral("以后「%1」%2都按 %3 天算吗？").arg(name, storage).arg(days);
}
inline QString ruleQuestionStorage(const QString &name, const QString &storage)
{
    return QStringLiteral("以后「%1」都建议%2存放吗？").arg(name, storage);
}

}
