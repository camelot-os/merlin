// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_API_GENERIC_H
#define MERLIN_PLATFORM_API_GENERIC_H

/*
 * merlin-based drivers aim to be used in a portable way by hosting applications.
 * To do that, we define in this header a set of types that can be reused by
 * various drivers (per device class), so that calling applications are able to
 * substitute one driver with another depending on the host platform,
 * without requiring code rewriting at application level.
 */

 /**
  * @brief driver status codes generic to all devices class
  *
  * This status codes aim to be used by all drivers, regardless of the device class,
  * in order to provide a common way to report errors and status to the calling application.
  *
  * Device drivers can still define their own status codes, but they should be defined as an extension
  * of the generic ones defined here, in order to maintain a common base for error handling across all drivers.
  * For example, a driver can define its own status codes starting from DRV_ERROR_UNSUPPORTED + 1, and so on.
  * This way, the calling application can still handle generic errors in a common way, while also being able
  * to handle device-specific errors when needed.
  */
typedef enum e_drv_status {
    DRV_STATUS_OK = 0, /**< Operation completed successfully */
    DRV_ERROR_INVSTATE = 1, /**< driver state incompatible with call (e.g. probe or init not done) */
    DRV_ERROR_INVPARAM = 2, /**< invalid parameter passed to the driver */
    DRV_ERROR_NOTREGISTERED = 3, /**< driver is not registered in the system */
    DRV_ERROR_CONFIGURATION = 4, /**< device configuration error (e.g. pinmux, clock, and so on) */
    DRV_ERROR_NACK = 5, /**< NACK received from the connected peripheral */
    DRV_ERROR_TIMEOUT = 6, /**< bus operation timed out */
    DRV_ERROR_BUS = 7, /**< bus error occurred */
    DRV_ERROR_NODEVICE = 8, /**< No device found at the specified address */
    DRV_ERROR_UNSUPPORTED_CFG = 9, /**< configuration is not supported by the device */
    DRV_ERROR_SETADDR_ERROR = 10, /**< device address setting operation failed */
    DRV_ERROR_SETCONFIG_ERROR = 11, /**< device configuration setting operation failed */
    DRV_ERROR_UNSUPPORTED = 12, /**< operation is not supported by the device */
    DRV_ERROR_AGAIN = 13, /**< operation failed but can be retried */
    DRV_ERROR_MAPFAILED = 14, /**< device memory mapping failed */
} drv_status_t;

#endif /* MERLIN_PLATFORM_API_GENERIC_H */
