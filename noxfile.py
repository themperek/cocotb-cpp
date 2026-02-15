from __future__ import annotations

import nox

nox.options.sessions = ["tests", "axil_ext_tests"]


def _install_base(session: nox.Session) -> None:
    session.install("pytest")
    session.install("pytest-timeout")
    session.install(".")


@nox.session
def tests(session: nox.Session) -> None:
    """Run core cocotb-cpp tests in an isolated environment."""
    _install_base(session)
    session.run("pytest", "-s", "tests")


@nox.session(name="axil_ext_tests")
def axil_ext_tests(session: nox.Session) -> None:
    """Run axil extension tests in an isolated environment."""
    _install_base(session)
    session.install("examples/axil_ext")
    session.run("pytest", "-s", "--timeout=120", "examples/axil_ext/tests")
