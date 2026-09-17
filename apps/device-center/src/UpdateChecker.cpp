#include "UpdateChecker.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>

namespace sony::devicecenter {

NetworkReleaseFetcher::NetworkReleaseFetcher(QObject* parent)
    : ReleaseFetcher(parent), _manager(new QNetworkAccessManager(this)) {
    _manager->setTransferTimeout(kTimeoutMs);
}

void NetworkReleaseFetcher::fetch(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("sony-device-center (%1)").arg(QSysInfo::prettyProductName()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = _manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // A 4xx/5xx still delivers a reply; only a transport failure has none.
        if (status == 0) emit finished(0, {}, reply->errorString());
        else emit finished(status, reply->readAll(), {});
    });
}

std::optional<UpdateChecker::Version> UpdateChecker::parseVersion(QString text) {
    static const QRegularExpression re(
        R"(^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$)");
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch()) return std::nullopt;
    Version v;
    v.major = m.captured(1).toInt();
    v.minor = m.captured(2).toInt();
    v.patch = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
    v.preRelease = m.captured(4);
    return v;
}

int UpdateChecker::compareVersions(const Version& a, const Version& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    // 1.0.0-rc1 < 1.0.0; between two pre-releases, semver's dot-separated
    // identifier order (numeric identifiers numerically, others as text).
    if (a.isPreRelease() != b.isPreRelease()) return a.isPreRelease() ? -1 : 1;
    const auto ia = a.preRelease.split('.'), ib = b.preRelease.split('.');
    for (int i = 0; i < std::min(ia.size(), ib.size()); ++i) {
        bool na = false, nb = false;
        const int va = ia[i].toInt(&na), vb = ib[i].toInt(&nb);
        if (na && nb) { if (va != vb) return va < vb ? -1 : 1; continue; }
        if (na != nb) return na ? -1 : 1;   // numeric identifiers sort first
        const int c = QString::compare(ia[i], ib[i]);
        if (c != 0) return c < 0 ? -1 : 1;
    }
    if (ia.size() != ib.size()) return ia.size() < ib.size() ? -1 : 1;
    return 0;
}

bool UpdateChecker::isNewer(const QString& candidate, const QString& current) {
    const auto c = parseVersion(candidate), cur = parseVersion(current);
    if (!c || !cur) return false;
    return compareVersions(*c, *cur) > 0;
}

QString UpdateChecker::platformAssetSuffix() {
#if defined(Q_OS_WIN)
    return "win64.msi";
#elif defined(Q_OS_MACOS)
    return "macOS.dmg";
#else
    return "Linux.deb";
#endif
}

UpdateChecker::UpdateChecker(QString currentVersion, ReleaseFetcher* fetcher, QObject* parent, QUrl apiUrl)
    : QObject(parent), _currentVersion(std::move(currentVersion)), _fetcher(fetcher), _apiUrl(std::move(apiUrl)) {
    _fetcher->setParent(this);
    connect(_fetcher, &ReleaseFetcher::finished, this, &UpdateChecker::_finished);
}

void UpdateChecker::check() {
    if (checking()) return;
    _set("checking");
    _fetcher->fetch(_apiUrl);
}

void UpdateChecker::_finished(int httpStatus, const QByteArray& body, const QString& error) {
    if (httpStatus == 0) { _set("failed", error.isEmpty() ? QStringLiteral("no reply") : error); return; }
    // GitHub answers 403 (or 429) with a JSON message when the unauthenticated
    // rate limit is spent; 404 means no release has been published yet.
    if (httpStatus != 200) { _set("failed", QStringLiteral("HTTP %1").arg(httpStatus)); return; }
    const auto root = QJsonDocument::fromJson(body).object();
    const auto tag = root.value("tag_name").toString();
    if (!parseVersion(tag)) { _set("failed", QStringLiteral("unrecognised tag \"%1\"").arg(tag)); return; }
    if (!isNewer(tag, _currentVersion)) { _set("upToDate"); return; }

    _latestVersion = tag.startsWith('v') ? tag.mid(1) : tag;
    _releaseUrl = root.value("html_url").toString();
    _downloadUrl.clear();
    const auto suffix = platformAssetSuffix();
    for (const auto& value : root.value("assets").toArray()) {
        const auto asset = value.toObject();
        if (asset.value("name").toString().endsWith(suffix)) {
            _downloadUrl = asset.value("browser_download_url").toString();
            break;
        }
    }
    _set("available");
    if (_announced != _latestVersion) {
        _announced = _latestVersion;
        emit updateAvailable(_latestVersion);
    }
}

void UpdateChecker::_set(const QString& state, const QString& error) {
    _state = state;
    _error = error;
    emit changed();
}

} // namespace sony::devicecenter
