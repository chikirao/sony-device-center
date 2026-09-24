#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace sony::devicecenter {

class I18nManager {
public:
    static I18nManager& instance();

    I18nManager();

    [[nodiscard]] QString translate(const QString& key, const QString& langCode) const;
    [[nodiscard]] QVariantList availableLanguages() const;
    // Every string of one language, for checks over the whole table.
    [[nodiscard]] QStringList strings(const QString& langCode) const { return _strings.value(langCode).values(); }

private:
    void _initTranslations();
    QMap<QString, QMap<QString, QString>> _strings;
};

} // namespace sony::devicecenter
