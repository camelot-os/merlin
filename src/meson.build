# SPDX-FileCopyrightText: 2023 - 2025 H2Lab OSS Team
# SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause

merlin_sourceset = ssmod.source_set()

merlin_common_src = files(
    './platform/platform_driver.c',
)

merlin_sourceset.add(merlin_common_src)
merlin_sourceset_config = merlin_sourceset.apply(kconfig_data, strict: false).sources()
