#pragma once

#include "UpdateChecker.h"

#include <QTimer>

namespace sony::devicecenter::test {

// A ReleaseFetcher that answers every fetch with the reply set up in
// advance, one event-loop turn later like a real one. No Q_OBJECT: it adds
// no signals of its own.
class FakeReleaseFetcher : public ReleaseFetcher {
public:
    using ReleaseFetcher::ReleaseFetcher;

    int httpStatus{200};
    QByteArray body;
    QString error;
    int fetches{0};
    QUrl lastUrl;

    static QByteArray release(const QString& tag, const QStringList& assetNames = {}) {
        QByteArray assets;
        for (const auto& name : assetNames) {
            if (!assets.isEmpty()) assets += ',';
            assets += QString(R"({"name":"%1","browser_download_url":"https://github.com/chikirao/sony-device-center/releases/download/%2/%1"})")
                          .arg(name, tag).toUtf8();
        }
        return QString(R"({"tag_name":"%1","html_url":"https://github.com/chikirao/sony-device-center/releases/tag/%1","assets":[%2]})")
            .arg(tag, QString::fromUtf8(assets)).toUtf8();
    }

    void fetch(const QUrl& url) override {
        ++fetches;
        lastUrl = url;
        QTimer::singleShot(0, this, [this] { emit finished(httpStatus, body, error); });
    }
};

} // namespace sony::devicecenter::test
