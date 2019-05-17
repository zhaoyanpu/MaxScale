/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2022-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

/**
 * @file ssl.hh
 *
 * The SSL definitions for MaxScale
 */

#include <maxscale/ccdefs.hh>
#include <maxscale/protocol.hh>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>

struct DCB;

enum ssl_method_type_t
{
#ifndef OPENSSL_1_1
    SERVICE_TLS10,
#endif
#ifdef OPENSSL_1_0
    SERVICE_TLS11,
    SERVICE_TLS12,
#endif
    SERVICE_SSL_MAX,
    SERVICE_TLS_MAX,
    SERVICE_SSL_TLS_MAX,
    SERVICE_SSL_UNKNOWN
};

/**
 * Return codes for SSL authentication checks
 */
#define SSL_AUTH_CHECKS_OK       0
#define SSL_ERROR_CLIENT_NOT_SSL 1
#define SSL_ERROR_ACCEPT_FAILED  2

namespace maxscale
{

/**
 * The ssl_listener structure is used to aggregate the SSL configuration items
 * and data for a particular listener
 */
struct SSLContext
{
    SSL_CTX*    ctx;
    SSL_METHOD* method;     /**<  SSLv3 or TLS1.0/1.1/1.2 methods
                             * see: https://www.openssl.org/docs/ssl/SSL_CTX_new.html */

    int               ssl_cert_verify_depth;/**< SSL certificate verification depth */
    ssl_method_type_t ssl_method_type;      /**< Which of the SSLv3 or TLS1.0/1.1/1.2 methods to use */

    char* ssl_cert;                     /**< SSL certificate */
    char* ssl_key;                      /**< SSL private key */
    char* ssl_ca_cert;                  /**< SSL CA certificate */
    bool  ssl_init_done;                /**< If SSL has already been initialized for this service */
    bool  ssl_verify_peer_certificate;  /**< Enable peer certificate verification */
};
}

int               ssl_authenticate_client(DCB* dcb, bool is_capable);
bool              ssl_is_connection_healthy(DCB* dcb);
bool              ssl_check_data_to_process(DCB* dcb);
bool              ssl_required_by_dcb(DCB* dcb);
bool              ssl_required_but_not_negotiated(DCB* dcb);
const char*       ssl_method_type_to_string(ssl_method_type_t method_type);
ssl_method_type_t string_to_ssl_method_type(const char* str);

/**
 * Helper function for client ssl authentication. Authenticates and checks changes
 * in ssl status.
 *
 * @param dcb Client dcb
 *
 * @return MXS_AUTH_FAILED_SSL or MXS_AUTH_FAILED on error. MXS_AUTH_SSL_INCOMPLETE
 * if ssl authentication is in progress and should be retried, MXS_AUTH_SSL_COMPLETE
 * if ssl authentication is complete or not required.
 */
int ssl_authenticate_check_status(DCB* dcb);

// TODO: Move this to an internal ssl.h header
void write_ssl_config(int fd, mxs::SSLContext* ssl);

/**
 * Set the maximum SSL/TLS version the listener will support
 *
 * @param ssl_listener Listener data to configure
 * @param version SSL/TLS version string
 *
 * @return  0 on success, -1 on invalid version string
 */
int listener_set_ssl_version(mxs::SSLContext* ssl_listener, const char* version);

/**
 * Set the locations of the listener's SSL certificate, listener's private key
 * and the CA certificate which both the client and the listener should trust.
 *
 * @param ssl_listener Listener data to configure
 * @param cert SSL certificate
 * @param key SSL private key
 * @param ca_cert SSL CA certificate
 */
void listener_set_certificates(mxs::SSLContext* ssl_listener, const std::string& cert,
                               const std::string& key, const std::string& ca_cert);

/**
 * Initialize SSL configuration
 *
 * This sets up the generated RSA encryption keys, chooses the listener
 * encryption level and configures the listener certificate, private key and
 * certificate authority file.
 *
 * @note This function should not be called directly, use config_create_ssl() instead
 *
 * @todo Combine this with config_create_ssl() into one function
 *
 * @param ssl SSL configuration to initialize
 *
 * @return True on success, false on error
 */
bool SSL_LISTENER_init(mxs::SSLContext* ssl);

/**
 * Free an SSL_LISTENER
 *
 * @param ssl mxs::SSLContext to free
 */
void SSL_LISTENER_free(mxs::SSLContext* ssl);
