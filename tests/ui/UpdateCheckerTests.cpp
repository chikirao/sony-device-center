#include <QtTest>
#include "UpdateChecker.h"
#include "../support/FakeReleaseFetcher.h"
using namespace sony::devicecenter;
using sony::devicecenter::test::FakeReleaseFetcher;

class UpdateCheckerTests : public QObject {
    Q_OBJECT

    static int cmp(const char* a, const char* b) {
        const auto va = UpdateChecker::parseVersion(a), vb = UpdateChecker::parseVersion(b);
        if (!va || !vb) return -99;
        return UpdateChecker::compareVersions(*va, *vb);
    }

private slots:
    void parsesTagsAndVersions() {
        auto v = UpdateChecker::parseVersion("v0.2.10");
        QVERIFY(v);
        QCOMPARE(v->major, 0); QCOMPARE(v->minor, 2); QCOMPARE(v->patch, 10);
        QVERIFY(!v->isPreRelease());
        v = UpdateChecker::parseVersion("1.2.3-rc.1+build.7");
        QVERIFY(v);
        QCOMPARE(v->preRelease, QString("rc.1"));
        // What the CMake fallback stamps into a tree configured on its own.
        v = UpdateChecker::parseVersion("0.0.0-dev");
        QVERIFY(v);
        QCOMPARE(v->preRelease, QString("dev"));
        QCOMPARE(UpdateChecker::parseVersion("1.2")->patch, 0);
        QVERIFY(!UpdateChecker::parseVersion("latest"));
        QVERIFY(!UpdateChecker::parseVersion("v1"));
        QVERIFY(!UpdateChecker::parseVersion(""));
    }

    void comparesNumericallyNotLexically() {
        QCOMPARE(cmp("0.2.1", "0.2.10"), -1);
        QCOMPARE(cmp("0.2.10", "0.2.9"), 1);
        QCOMPARE(cmp("0.10.0", "0.9.9"), 1);
        QCOMPARE(cmp("1.0.0", "0.99.99"), 1);
        QCOMPARE(cmp("v0.2.1", "0.2.1"), 0);
    }

    void preReleaseSortsBeforeItsFinal() {
        QCOMPARE(cmp("1.0.0-rc1", "1.0.0"), -1);
        QCOMPARE(cmp("1.0.0", "1.0.0-rc1"), 1);
        QCOMPARE(cmp("1.0.0-rc1", "0.9.0"), 1);
        QCOMPARE(cmp("1.0.0-alpha", "1.0.0-beta"), -1);
        QCOMPARE(cmp("1.0.0-rc.2", "1.0.0-rc.10"), -1);
        QCOMPARE(cmp("1.0.0-rc.1", "1.0.0-rc.1.1"), -1);
        QCOMPARE(cmp("0.0.0-dev", "0.2.1"), -1);
    }

    void isNewerRefusesWhatItCannotParse() {
        QVERIFY(UpdateChecker::isNewer("v0.2.10", "0.2.1"));
        QVERIFY(!UpdateChecker::isNewer("v0.2.1", "0.2.1"));
        QVERIFY(!UpdateChecker::isNewer("v0.2.0", "0.2.1"));
        QVERIFY(!UpdateChecker::isNewer("nightly", "0.2.1"));
        QVERIFY(!UpdateChecker::isNewer("v0.3.0", "garbage"));
    }

    void newerReleaseIsAvailableWithThisPlatformsAsset() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->body = FakeReleaseFetcher::release("v0.3.0", {"sony-device-center-0.3.0-Linux.deb", "sony-device-center-0.3.0-Linux.rpm",
                                                               "sony-device-center-0.3.0-macOS.dmg", "sony-device-center-0.3.0-win64.msi",
                                                               "sony-device-center-0.3.0-win64.zip"});
        UpdateChecker checker("0.2.1", fetcher);
        QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
        QCOMPARE(checker.state(), QString("idle"));
        checker.check();
        QCOMPARE(checker.state(), QString("checking"));
        QVERIFY(checker.checking());
        QCOMPARE(fetcher->lastUrl, UpdateChecker::kDefaultApiUrl);
        QTRY_COMPARE(checker.state(), QString("available"));
        QVERIFY(checker.available());
        QCOMPARE(checker.latestVersion(), QString("0.3.0"));
        QCOMPARE(checker.releaseUrl(), QString("https://github.com/chikirao/sony-device-center/releases/tag/v0.3.0"));
        QCOMPARE(checker.downloadUrl(),
                 QString("https://github.com/chikirao/sony-device-center/releases/download/v0.3.0/sony-device-center-0.3.0-%1")
                     .arg(UpdateChecker::platformAssetSuffix()));
        QCOMPARE(available.count(), 1);
        QCOMPARE(available.first().first().toString(), QString("0.3.0"));
        // Checking again finds the same release: the card updates, the
        // toast does not repeat.
        checker.check();
        QTRY_COMPARE(fetcher->fetches, 2);
        QTRY_COMPARE(checker.state(), QString("available"));
        QCOMPARE(available.count(), 1);
    }

    void releaseWithoutAnInstallerOffersOnlyThePage() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->body = FakeReleaseFetcher::release("v0.3.0", {"sony-device-center-0.3.0-Source.tar.gz"});
        UpdateChecker checker("0.2.1", fetcher);
        checker.check();
        QTRY_COMPARE(checker.state(), QString("available"));
        QVERIFY(checker.downloadUrl().isEmpty());
        QVERIFY(!checker.releaseUrl().isEmpty());
    }

    void sameOrOlderReleaseMeansUpToDate() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->body = FakeReleaseFetcher::release("v0.2.1");
        UpdateChecker checker("0.2.1", fetcher);
        QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
        checker.check();
        QTRY_COMPARE(checker.state(), QString("upToDate"));
        QVERIFY(!checker.available());
        QVERIFY(checker.latestVersion().isEmpty());
        // A dev build ahead of the last tag is not told to "update".
        auto* devFetcher = new FakeReleaseFetcher;
        devFetcher->body = FakeReleaseFetcher::release("v0.2.1");
        UpdateChecker dev("0.3.0", devFetcher);
        dev.check();
        QTRY_COMPARE(dev.state(), QString("upToDate"));
        QCOMPARE(available.count(), 0);
    }

    void networkFailureFailsQuietly() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->httpStatus = 0;
        fetcher->error = "Host api.github.com not found";
        UpdateChecker checker("0.2.1", fetcher);
        QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
        checker.check();
        QTRY_COMPARE(checker.state(), QString("failed"));
        QCOMPARE(checker.error(), QString("Host api.github.com not found"));
        QVERIFY(!checker.available());
        QCOMPARE(available.count(), 0);
        // The user can try again by hand once the network is back.
        fetcher->httpStatus = 200;
        fetcher->body = FakeReleaseFetcher::release("v0.2.2", {"sony-device-center-0.2.2-" + UpdateChecker::platformAssetSuffix()});
        checker.check();
        QTRY_COMPARE(checker.state(), QString("available"));
        QCOMPARE(available.count(), 1);
    }

    void rateLimitAndOtherHttpErrorsFail() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->httpStatus = 403;
        fetcher->body = R"({"message":"API rate limit exceeded for 1.2.3.4.","documentation_url":"https://docs.github.com/rest/overview/resources-in-the-rest-api#rate-limiting"})";
        UpdateChecker checker("0.2.1", fetcher);
        checker.check();
        QTRY_COMPARE(checker.state(), QString("failed"));
        QCOMPARE(checker.error(), QString("HTTP 403"));
        // No release published at all.
        fetcher->httpStatus = 404;
        fetcher->body = R"({"message":"Not Found"})";
        checker.check();
        QTRY_COMPARE(fetcher->fetches, 2);
        QTRY_COMPARE(checker.state(), QString("failed"));
        QCOMPARE(checker.error(), QString("HTTP 404"));
    }

    void garbageBodyFails() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->body = "<html>maintenance</html>";
        UpdateChecker checker("0.2.1", fetcher);
        checker.check();
        QTRY_COMPARE(checker.state(), QString("failed"));
        QVERIFY(checker.error().contains("unrecognised tag"));
    }

    void checkWhileCheckingIsIgnored() {
        auto* fetcher = new FakeReleaseFetcher;
        fetcher->body = FakeReleaseFetcher::release("v0.2.1");
        UpdateChecker checker("0.2.1", fetcher);
        checker.check();
        checker.check();
        QCOMPARE(fetcher->fetches, 1);
        QTRY_COMPARE(checker.state(), QString("upToDate"));
    }

    void platformAssetSuffixMatchesTheReleaseWorkflow() {
#if defined(Q_OS_WIN)
        QCOMPARE(UpdateChecker::platformAssetSuffix(), QString("win64.msi"));
#elif defined(Q_OS_MACOS)
        QCOMPARE(UpdateChecker::platformAssetSuffix(), QString("macOS.dmg"));
#else
        QCOMPARE(UpdateChecker::platformAssetSuffix(), QString("Linux.deb"));
#endif
    }
};

QTEST_GUILESS_MAIN(UpdateCheckerTests)
#include "UpdateCheckerTests.moc"
