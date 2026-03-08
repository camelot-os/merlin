# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab Development Team

merlin_sourceset = ssmod.source_set()

merlin_private_headers = []
merlin_common_src = []

subdir('buses')
subdir('platform')

merlin_sourceset.add(merlin_common_src)
merlin_sourceset_config = merlin_sourceset.apply(kconfig_data, strict: false).sources()
