import os
import pytest

# We set up hooks to count how many tests actually ran. Since we need uinput
# for the tests, it's likely they all get skipped when we don't run as root or
# uinput isn't available.
# If all tests are skipped, we want to exit with 77, not success


def pytest_sessionstart(session):
    session.count_not_skipped = 0
    session.count_skipped = 0


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    result = outcome.get_result()

    if result.when == "call":
        if result.skipped:
            item.session.count_skipped += 1
        else:
            item.session.count_not_skipped += 1


def pytest_sessionfinish(session, exitstatus):
    if session.count_not_skipped == 0 and session.count_skipped > 0:
        session.exitstatus = 77
        reporter = session.config.pluginmanager.get_plugin("terminalreporter")
        reporter.section("Session errors", sep="-", red=True, bold=True)
        reporter.line(f"{session.count_skipped} tests were skipped, none were run")


@pytest.fixture(autouse=True)
def set_environment():
    os.environ["WACOM_RUNNING_TEST_SUITE"] = "1"


# vim: set expandtab tabstop=4 shiftwidth=4:
