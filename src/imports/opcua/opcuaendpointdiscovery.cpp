/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt OPC UA module.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "opcuaendpointdiscovery.h"
#include "opcuaconnection.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype EndpointDiscovery
    \inqmlmodule QtOpcUa
    \brief Provides information about available endpoints on a server.
    \since QtOpcUa 5.13

    Allows to fetch and access information about available endpoints on a server.

    \snippet ../../src/imports/doc/snippets/basic/basic.qml Basic discovery
*/

/*!
    \qmlproperty string EndointDiscovery::serverUrl

    Discovery URL of the server to fetch the endpoints from.
    Every time the URL is changed, a request to the given server is started.

    When starting the request, the list of available endpoints is cleared and the status
    is set to \l Status.GoodCompletesAsynchronously. Once the request is finished, \l status changes.
    Make sure to check \l status before acessing the list of endpoints.

    \sa onEndpointsChanged
*/

/*!
    \qmlproperty int EndpointDiscovery::count

    Current number of endpoints in this element.
    Before using any data from an endpoint discovery, you should check \l status if the information
    was successfully retrieved.

    \sa status Status
*/

/*!
    \qmlproperty Status EndpointDiscovery::status

    The current status of this element.
    In case the last retrieval of endpoints was successful, the status
    is \c Status.Good.

    \code
    if (endpoints.status.isGood) {
        // do something
    } else {
        // handle error
    }
    \endcode

    \sa Status
*/

/*!
    \qmlsignal EndpointDiscovery::endpointsChanged()

    Emitted when a retrieval request started, finished or failed.
    In a called function, you should first check the \l status of the object.
    In case the status is \l Status.GoodCompletesAsynchronously, the request is still running.
    In case the status is \l Status.Good, the request has finished and the endpoint descriptions
    can be read. In case the status is not good, an error happended and \l status contains the
    returned error code.

    \code
    onEndpointsChanged: {
            if (endpoints.status.isGood) {
                if (endpoints.status.status == QtOpcua.Status.GoodCompletesAsynchronusly)
                    return; // wait until finished
                if (enpoints.count > 0) {
                    var endpointUrl = enpoints.at(0).endpointUrl();
                    console.log(endpointUrl);
                }
            } else {
                // handle error
            }
    }
    \endcode

    \sa status count at Status EndpointDescription
*/

/*!
    \qmlproperty Connection EndpointDiscovery::connection

    The connection to be used for requesting information.

    If this property is not set, the default connection will be used, if any.

    \sa Connection, Connection::defaultConnection
*/

OpcUaEndpointDiscovery::OpcUaEndpointDiscovery(QObject *parent)
    : QObject(parent)
{
    connect(this, &OpcUaEndpointDiscovery::serverUrlChanged, this, &OpcUaEndpointDiscovery::startRequestEndpoints);
    connect(this, &OpcUaEndpointDiscovery::connectionChanged, this, &OpcUaEndpointDiscovery::startRequestEndpoints);
}

OpcUaEndpointDiscovery::~OpcUaEndpointDiscovery() = default;

const QString &OpcUaEndpointDiscovery::serverUrl() const
{
    return m_serverUrl;
}

void OpcUaEndpointDiscovery::setServerUrl(const QString &serverUrl)
{
    if (serverUrl == m_serverUrl)
        return;

    m_serverUrl = serverUrl;
    emit serverUrlChanged(m_serverUrl);
}

int OpcUaEndpointDiscovery::count() const
{
    return m_endpoints.count();
}

/*!
    \qmlmethod EndpointDescription EndpointDiscovery::at(index)

    Returns the endpoint description at given \a index.
    In case there are no endoints available or the index is invalid, an invalid
    endpoint description is returned.
    Before using any data from this, you should check \l status if retrieval of the
    information was successful.

    \code
    if (endpoints.status.isGood) {
        if (endpoints.count > 0)
            var endpointUrl = endpoints.at(0).endpointUrl();
            console.log(endpointUrl);
    } else {
        // handle error
    }
    \endcode

    \sa count status EndpointDescription
*/

QOpcUaEndpointDescription OpcUaEndpointDiscovery::at(int row) const
{
    if (row >= m_endpoints.count())
        return QOpcUaEndpointDescription();
    return m_endpoints.at(row);
}

const OpcUaStatus &OpcUaEndpointDiscovery::status() const
{
    return m_status;
}

void OpcUaEndpointDiscovery::connectSignals()
{
    auto conn = connection();

    if (!conn || !conn->m_client)
        return;
    connect(conn->m_client, &QOpcUaClient::endpointsRequestFinished, this, &OpcUaEndpointDiscovery::handleEndpoints, Qt::UniqueConnection);
}

void OpcUaEndpointDiscovery::handleEndpoints(const QVector<QOpcUaEndpointDescription> &endpoints, QOpcUa::UaStatusCode statusCode, const QUrl &requestUrl)
{
    if (requestUrl != m_serverUrl)
        return; // response is not for last request

    m_status = OpcUaStatus(statusCode);

    if (m_status.isBad()) {
        emit statusChanged();
        return;
    }

    m_endpoints = endpoints;
    emit endpointsChanged();
    emit countChanged();
    emit statusChanged();
}

void OpcUaEndpointDiscovery::startRequestEndpoints()
{
    if (!m_componentCompleted)
        return;

    if (m_serverUrl.isEmpty())
        return;

    m_endpoints.clear();

    if (!m_connection) {
        // If there is not connection set, try the default connection
        // Any connection change will restart this function
        connection();
        return;
    }

    auto conn = connection();

    if (!conn || !conn->m_client) {
        m_status = OpcUaStatus(QOpcUa::BadNotConnected);
    } else if (m_serverUrl.isEmpty()) {
        m_status = OpcUaStatus(QOpcUa::BadInvalidArgument);
    } else {
        m_status = OpcUaStatus(QOpcUa::GoodCompletesAsynchronously);
        conn->m_client->requestEndpoints(m_serverUrl);
    }

    emit endpointsChanged();
    emit statusChanged();
}

void OpcUaEndpointDiscovery::setConnection(OpcUaConnection *connection)
{
    if (connection == m_connection || !connection)
        return;

    if (m_connection)
        disconnect(m_connection, &OpcUaConnection::backendChanged, this, &OpcUaEndpointDiscovery::connectSignals);

    m_connection = connection;

    connect(m_connection, &OpcUaConnection::backendChanged, this, &OpcUaEndpointDiscovery::connectSignals, Qt::UniqueConnection);
    connectSignals();
    emit connectionChanged(connection);
}

OpcUaConnection *OpcUaEndpointDiscovery::connection()
{
    if (!m_connection)
        setConnection(OpcUaConnection::defaultConnection());

    return m_connection;
}

void OpcUaEndpointDiscovery::classBegin()
{
}

void OpcUaEndpointDiscovery::componentComplete()
{
    m_componentCompleted = true;
    startRequestEndpoints();
}

QT_END_NAMESPACE
