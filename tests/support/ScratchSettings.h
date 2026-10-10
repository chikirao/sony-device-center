#pragma once

#include "AppSettings.h"

#include <QFileInfo>
#include <QTemporaryDir>

#include <memory>

namespace sony::devicecenter::test {

// Sends AppSettings to a scratch directory, so a test run, passing or
// failing, never reads or writes the user's real preferences. Call reset()
// from init(): each test then starts from the defaults in an empty
// directory of its own. It returns false when AppSettings would still land
// outside that directory, and the test must not run.
class ScratchSettings {
public:
    bool reset() {
        _dir = std::make_unique<QTemporaryDir>();
        if (!_dir->isValid()) return false;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, _dir->path());
        // The machine-wide fallback (/etc/xdg, ProgramData) is read too.
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, _dir->filePath("system"));
        // QSettings makes its file name absolute; compare it the same way.
        return AppSettings().fileName().startsWith(QFileInfo(_dir->path()).absoluteFilePath() + '/');
    }

private:
    std::unique_ptr<QTemporaryDir> _dir;
};

} // namespace sony::devicecenter::test
