#! /usr/bin/env python3
# Copyright (C) 2026 fontconfig Authors
# SPDX-License-Identifier: HPND

import pytest
from fctest import FcTest, FcTestFont


@pytest.fixture
def fctest():
    return FcTest()


@pytest.fixture
def fcfont():
    return FcTestFont()


def test_cache_kept_with_source_date_epoch(fctest, fcfont):
    """fc-cache must not discard its own cache when the font directory is
    newer than SOURCE_DATE_EPOCH"""
    fctest.setup()
    # Install without SOURCE_DATE_EPOCH so that the directory mtime is "now",
    # i.e. newer than the epoch used below.
    fctest.install_font(fcfont.fonts, ".")
    fctest.env["SOURCE_DATE_EPOCH"] = "1000000000"

    for ret, stdout, stderr in fctest.run_cache(
        ["-f", fctest.convert_path(fctest.fontdir.name)]
    ):
        assert ret == 0, stderr
    assert len(list(fctest.cache_files())) > 0
