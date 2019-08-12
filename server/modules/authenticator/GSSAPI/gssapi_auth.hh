#pragma once
/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#define MXS_MODULE_NAME "GSSAPIAuth"

#include <maxscale/ccdefs.hh>
#include <stdint.h>
#include <stddef.h>
#include <gssapi.h>
#include <maxscale/sqlite3.h>
#include <maxscale/authenticator2.hh>

/** Client auth plugin name */
static const char auth_plugin_name[] = "auth_gssapi_client";

/** This is mainly for testing purposes */
static const char default_princ_name[] = "mariadb/localhost.localdomain";

/** GSSAPI authentication states */
enum gssapi_auth_state
{
    GSSAPI_AUTH_INIT = 0,
    GSSAPI_AUTH_DATA_SENT,
    GSSAPI_AUTH_OK,
    GSSAPI_AUTH_FAILED
};

/** Report GSSAPI errors */
void report_error(OM_uint32 major, OM_uint32 minor);

class GSSAPIAuthenticatorModule : public mxs::AuthenticatorModule
{
public:
    static GSSAPIAuthenticatorModule* create(char** options);
    ~GSSAPIAuthenticatorModule() override = default;
    std::unique_ptr<mxs::ClientAuthenticator> create_client_authenticator() override;
    int load_users(Listener* listener) override;
    void diagnostics(DCB* output, Listener* listener) override;
    json_t* diagnostics_json(const Listener* listener) override;
    uint64_t capabilities() const override;

    char*    principal_name {nullptr}; /**< Service principal name given to the client */

private:
    sqlite3* handle {nullptr};         /**< SQLite3 database handle */
};

class GSSAPIClientAuthenticator : public mxs::ClientAuthenticator
{
public:
    ~GSSAPIClientAuthenticator() override;
    bool extract(DCB* client, GWBUF* buffer) override;
    bool ssl_capable(DCB* client) override;
    int authenticate(DCB* client) override;
    void free_data(DCB* client) override;

    std::unique_ptr<mxs::BackendAuthenticator> create_backend_authenticator() override;
    sqlite3*          handle {nullptr};            /**< SQLite3 database handle */
    uint8_t           sequence {0};                /**< The next packet seqence number */

private:
    void copy_client_information(DCB* dcb, GWBUF* buffer);
    bool store_client_token(DCB* dcb, GWBUF* buffer);

    gssapi_auth_state state {GSSAPI_AUTH_INIT};    /**< Authentication state*/
    uint8_t*          principal_name {nullptr};    /**< Principal name */
};

class GSSAPIBackendAuthenticator : public mxs::BackendAuthenticator
{
public:
    ~GSSAPIBackendAuthenticator() override;
    bool extract(DCB* backend, GWBUF* buffer) override;
    bool ssl_capable(DCB* backend) override;
    int authenticate(DCB* backend) override;

private:
    bool extract_principal_name(DCB* dcb, GWBUF* buffer);
    bool send_new_auth_token(DCB* dcb);

    gssapi_auth_state state {GSSAPI_AUTH_INIT};      /**< Authentication state*/
    uint8_t*          principal_name {nullptr};      /**< Principal name */
    uint8_t           sequence {0};                  /**< The next packet sequence number */
    sqlite3*          handle {nullptr};              /**< SQLite3 database handle */
};
