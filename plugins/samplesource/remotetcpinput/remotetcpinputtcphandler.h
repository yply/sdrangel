///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_
#define PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QMutex>
#include <QDateTime>

#include "util/messagequeue.h"
#include "remotetcpinputsettings.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"

class SampleSinkFifo;
class MessageQueue;
class DeviceAPI;

class RemoteTCPInputTCPHandler : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureTcpHandler : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteTCPInputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureTcpHandler* create(const RemoteTCPInputSettings& settings, bool force)
        {
            return new MsgConfigureTcpHandler(settings, force);
        }

    private:
        RemoteTCPInputSettings m_settings;
        bool m_force;

        MsgConfigureTcpHandler(const RemoteTCPInputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportRemoteDevice : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        RemoteTCPProtocol::Device getDevice() const { return m_device; }
        QString getProtocol() const { return m_protocol; }

        static MsgReportRemoteDevice* create(RemoteTCPProtocol::Device device, const QString& protocol)
        {
            return new MsgReportRemoteDevice(device, protocol);
        }

    protected:
        RemoteTCPProtocol::Device m_device;
        QString m_protocol;

        MsgReportRemoteDevice(RemoteTCPProtocol::Device device, const QString& protocol) :
            Message(),
            m_device(device),
            m_protocol(protocol)
        { }
    };

    class MsgReportConnection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getConnected() const { return m_connected; }

        static MsgReportConnection* create(bool connected)
        {
            return new MsgReportConnection(connected);
        }

    protected:
        bool m_connected;

        MsgReportConnection(bool connected) :
            Message(),
            m_connected(connected)
        { }
    };

    RemoteTCPInputTCPHandler(SampleSinkFifo* sampleFifo, DeviceAPI *deviceAPI);
    ~RemoteTCPInputTCPHandler();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToInput(MessageQueue *queue) { m_messageQueueToInput = queue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    void reset();
    void start();
    void stop();
    int getBufferGauge() const { return 0; }

public slots:
    void dataReadyRead();
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);

private:

    DeviceAPI *m_deviceAPI;
    bool m_running;
    QTcpSocket *m_dataSocket;
    char *m_tcpBuf;
    SampleSinkFifo *m_sampleFifo;
    MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_messageQueueToInput;
    MessageQueue *m_messageQueueToGUI;
    bool m_readMetaData;
    bool m_fillBuffer;
    QTimer m_timer;
    QTimer m_reconnectTimer;
    QDateTime m_prevDateTime;

    int32_t *m_converterBuffer;
    uint32_t m_converterBufferNbSamples;

    QMutex m_mutex;
    RemoteTCPInputSettings m_settings;

    void applyTCPLink(const QString& address, quint16 port);
    bool handleMessage(const Message& message);
    void convert(int nbSamples);
    void connectToHost(const QString& address, quint16 port);
    void disconnectFromHost();
    void cleanup();
    void clearBuffer();
    void setSampleRate(int sampleRate);
    void setCenterFrequency(quint64 frequency);
    void setTunerAGC(bool agc);
    void setTunerGain(int gain);
    void setFreqCorrection(int correction);
    void setIFGain(quint16 stage, quint16 gain);
    void setAGC(bool agc);
    void setDirectSampling(bool enabled);
    void setDCOffsetRemoval(bool enabled);
    void setIQCorrection(bool enabled);
    void setBiasTee(bool enabled);
    void setBandwidth(int bandwidth);
    void setDecimation(int dec);
    void setChannelSampleRate(int dec);
    void setChannelFreqOffset(int offset);
    void setChannelGain(int gain);
    void setSampleBitDepth(int sampleBits);
    void applySettings(const RemoteTCPInputSettings& settings, bool force = false);

private slots:
    void started();
    void finished();
    void handleInputMessages();
    void processData();
    void reconnect();
};

#endif /* PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_ */
