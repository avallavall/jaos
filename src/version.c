/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

const char *jaos_version(void)
{
    return JAOS_VERSION_STRING;
}

const char *jaos_build_commit(void)
{
#ifdef JAOS_BUILD_COMMIT
    return JAOS_BUILD_COMMIT;
#else
    return "";
#endif
}
