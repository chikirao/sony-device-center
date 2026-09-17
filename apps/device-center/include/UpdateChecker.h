#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;

namespace sony::devicecenter {

// The one network call the update check makes: GET a URL, hand back the
// status code and body. Abstract so tests feed canned replies and never
// reach GitHub.
class ReleaseFetcher : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void fetch(const QUrl& url) = 0;

signals:
    // httpStatus is 0 when no reply arrived at all (offline, DNS, timeout);
    // error then says why. Otherwise body is whatever the server sent, even
    // for a 4xx/5xx.
    void finished(int httpStatus, const QByteArray& body, const QString& error);
};

// QNetworkAccessManager behind the interface, with a short transfer timeout
// so a missing network never holds anything up.
class NetworkReleaseFetcher : public ReleaseFetcher {
    Q_OBJECT
public:
    static constexpr int kTimeoutMs = 5000;
    explicit NetworkReleaseFetcher(QObject* parent = nullptr);
    void fetch(const QUrl& url) override;

private:
    QNetworkAccessManager* _manager;
};

// Asks the GitHub Releases API whether a newer version than the running one
// has been published, and where its installer for this platform lives.
// Owns no settings: whoever creates it decides when to call check().
class UpdateChecker : public QObject {
    Q_OBJECT
    // "idle", "checking", "upToDate", "available" or "failed".
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY changed)
    // Direct link to this platform's package, or empty when the release
    // carries none (then only the release page can be offered).
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    struct Version {
        int major{0}, minor{0}, patch{0};
        QString preRelease;   // "rc1" in 1.2.0-rc1; empty for a final release
        [[nodiscard]] bool isPreRelease() const { return !preRelease.isEmpty(); }
    };

    static inline const QUrl kDefaultApiUrl{"https://api.github.com/repos/chikirao/sony-device-center/releases/latest"};

    // "v1.2.3", "1.2.3-rc1" or "1.2.3+build" (build metadata ignored).
    // Anything else, including "0.0.0-dev" style dev builds' suffix, still
    // parses as long as the numbers are there.
    static std::optional<Version> parseVersion(QString text);
    // Semver order: numbers, then a pre-release sorts before its final.
    static int compareVersions(const Version& a, const Version& b);
    static bool isNewer(const QString& candidate, const QString& current);
    // "win64.msi", "macOS.dmg" or "Linux.deb": the suffix of the release
    // asset that installs on this platform.
    static QString platformAssetSuffix();

    // Takes ownership of the fetcher.
    UpdateChecker(QString currentVersion, ReleaseFetcher* fetcher, QObject* parent = nullptr,
                  QUrl apiUrl = kDefaultApiUrl);

    Q_INVOKABLE void check();

    [[nodiscard]] QString state() const { return _state; }
    [[nodiscard]] bool checking() const { return _state == "checking"; }
    [[nodiscard]] bool available() const { return _state == "available"; }
    [[nodiscard]] QString currentVersion() const { return _currentVersion; }
    [[nodiscard]] QString latestVersion() const { return _latestVersion; }
    [[nodiscard]] QString releaseUrl() const { return _releaseUrl; }
    [[nodiscard]] QString downloadUrl() const { return _downloadUrl; }
    [[nodiscard]] QString error() const { return _error; }

signals:
    void changed();
    // Once per newer version seen in this run, so a manual re-check does
    // not toast the same release again.
    void updateAvailable(const QString& version);

private:
    void _finished(int httpStatus, const QByteArray& body, const QString& error);
    void _set(const QString& state, const QString& error = {});

    QString _currentVersion;
    ReleaseFetcher* _fetcher;
    QUrl _apiUrl;
    QString _state{"idle"};
    QString _latestVersion, _releaseUrl, _downloadUrl, _error;
    QString _announced;
};

} // namespace sony::devicecenter
