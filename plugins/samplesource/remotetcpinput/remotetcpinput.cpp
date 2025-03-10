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

#include <string.h>
#include <errno.h>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QJsonParseError>

#include "SWGDeviceSettings.h"
#include "SWGChannelSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGRemoteTCPInputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "remotetcpinput.h"
#include "remotetcpinputtcphandler.h"

MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgConfigureRemoteTCPInput, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgReportTCPBuffer, Message)

RemoteTCPInput::RemoteTCPInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_mutex(QMutex::Recursive),
    m_settings(),
    m_remoteInputTCPPHandler(nullptr),
    m_deviceDescription("RemoteTCPInput")
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_sampleFifo.setSize(48000 * 8);
    m_remoteInputTCPPHandler = new RemoteTCPInputTCPHandler(&m_sampleFifo, m_deviceAPI);
    m_remoteInputTCPPHandler->moveToThread(&m_thread);
    m_remoteInputTCPPHandler->setMessageQueueToInput(&m_inputMessageQueue);

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RemoteTCPInput::networkManagerFinished
    );
}

RemoteTCPInput::~RemoteTCPInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RemoteTCPInput::networkManagerFinished
    );
    delete m_networkManager;
    stop();
    m_remoteInputTCPPHandler->deleteLater();
}

void RemoteTCPInput::destroy()
{
    delete this;
}

void RemoteTCPInput::init()
{
    applySettings(m_settings, true);
}

bool RemoteTCPInput::start()
{
    qDebug() << "RemoteTCPInput::start";
    m_remoteInputTCPPHandler->reset();
    m_remoteInputTCPPHandler->start();
    m_remoteInputTCPPHandler->getInputMessageQueue()->push(RemoteTCPInputTCPHandler::MsgConfigureTcpHandler::create(m_settings, true));
    m_thread.start();
    return true;
}

void RemoteTCPInput::stop()
{
    qDebug() << "RemoteTCPInput::stop";
    m_remoteInputTCPPHandler->stop();
    m_thread.quit();
    m_thread.wait();
}

QByteArray RemoteTCPInput::serialize() const
{
    return m_settings.serialize();
}

bool RemoteTCPInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureRemoteTCPInput* message = MsgConfigureRemoteTCPInput::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRemoteTCPInput* messageToGUI = MsgConfigureRemoteTCPInput::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void RemoteTCPInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    m_remoteInputTCPPHandler->setMessageQueueToGUI(queue);
}

const QString& RemoteTCPInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int RemoteTCPInput::getSampleRate() const
{
    return m_settings.m_channelSampleRate;
}

quint64 RemoteTCPInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + m_settings.m_inputFrequencyOffset;
}

void RemoteTCPInput::setCenterFrequency(qint64 centerFrequency)
{
    RemoteTCPInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureRemoteTCPInput* message = MsgConfigureRemoteTCPInput::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRemoteTCPInput* messageToGUI = MsgConfigureRemoteTCPInput::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool RemoteTCPInput::handleMessage(const Message& message)
{
    if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "RemoteTCPInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (MsgConfigureRemoteTCPInput::match(message))
    {
        qDebug() << "RemoteTCPInput::handleMessage:" << message.getIdentifier();
        MsgConfigureRemoteTCPInput& conf = (MsgConfigureRemoteTCPInput&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
    else if (RemoteTCPInputTCPHandler::MsgReportConnection::match(message))
    {
        qDebug() << "RemoteTCPInput::handleMessage:" << message.getIdentifier();
        RemoteTCPInputTCPHandler::MsgReportConnection& report = (RemoteTCPInputTCPHandler::MsgReportConnection&) message;
        if (report.getConnected())
        {
            qDebug() << "Disconnected - stopping DSP";
            m_deviceAPI->stopDeviceEngine();
        }
        return true;
    }
    else
    {
        return false;
    }
}

void RemoteTCPInput::applySettings(const RemoteTCPInputSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    QList<QString> reverseAPIKeys;
    bool forwardChange = false;

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        reverseAPIKeys.append("centerFrequency");
        forwardChange = true;
    }
    if ((m_settings.m_loPpmCorrection != settings.m_loPpmCorrection) || force) {
        reverseAPIKeys.append("loPpmCorrection");
    }
    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        reverseAPIKeys.append("dcBlock");
    }
    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        reverseAPIKeys.append("iqCorrection");
    }
    if ((m_settings.m_biasTee != settings.m_biasTee) || force) {
        reverseAPIKeys.append("biasTee");
    }
    if ((m_settings.m_directSampling != settings.m_directSampling) || force) {
        reverseAPIKeys.append("noModMode"); // Use same name as rtlsdrinput.cpp
    }
    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force) {
        reverseAPIKeys.append("devSampleRate");
    }
    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((m_settings.m_gain != settings.m_gain) || force) {
        reverseAPIKeys.append("gain");
    }
    if ((m_settings.m_agc != settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }
    if ((m_settings.m_rfBW != settings.m_rfBW) || force) {
        reverseAPIKeys.append("rfBW");
    }
    if ((m_settings.m_rfBW != settings.m_rfBW) || force) {
        reverseAPIKeys.append("rfBW");
    }
    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force)
    {
        reverseAPIKeys.append("inputFrequencyOffset");
        forwardChange = true;
    }
    if ((m_settings.m_channelGain != settings.m_channelGain) || force) {
        reverseAPIKeys.append("channelGain");
    }
    if ((m_settings.m_channelSampleRate != settings.m_channelSampleRate) || force)
    {
        reverseAPIKeys.append("channelSampleRate");
        forwardChange = true;
    }
    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((m_settings.m_sampleBits != settings.m_sampleBits) || force) {
        reverseAPIKeys.append("m_sampleBits");
    }
    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        reverseAPIKeys.append("dataAddress");
    }
    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        reverseAPIKeys.append("dataPort");
    }
    if ((m_settings.m_preFill != settings.m_preFill) || force) {
        reverseAPIKeys.append("preFill");
    }

    mutexLocker.unlock();

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    if (forwardChange && (settings.m_channelSampleRate != 0))
    {
        DSPSignalNotification *notif = new DSPSignalNotification(settings.m_channelSampleRate, settings.m_centerFrequency + settings.m_inputFrequencyOffset);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    m_settings = settings;

    m_remoteInputTCPPHandler->getInputMessageQueue()->push(RemoteTCPInputTCPHandler::MsgConfigureTcpHandler::create(m_settings, force));

    qDebug() << "RemoteTCPInput::applySettings: "
        << " force: " << force
        << " m_centerFrequency: " << m_settings.m_centerFrequency
        << " m_loPpmCorrection: " << m_settings.m_loPpmCorrection
        << " m_biasTee: " << m_settings.m_biasTee
        << " m_devSampleRate: " << m_settings.m_devSampleRate
        << " m_log2Decim: " << m_settings.m_log2Decim
        << " m_gain: " << m_settings.m_gain
        << " m_agc: " << m_settings.m_agc
        << " m_rfBW: " << m_settings.m_rfBW
        << " m_inputFrequencyOffset: " << m_settings.m_inputFrequencyOffset
        << " m_channelGain: " << m_settings.m_channelGain
        << " m_channelSampleRate: " << m_settings.m_channelSampleRate
        << " m_sampleBits: " << m_settings.m_sampleBits
        << " m_dataAddress: " << m_settings.m_dataAddress
        << " m_dataPort: " << m_settings.m_dataPort
        << " m_preFill: " << m_settings.m_preFill
        ;
}

int RemoteTCPInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int RemoteTCPInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

int RemoteTCPInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteTcpInputSettings(new SWGSDRangel::SWGRemoteTCPInputSettings());
    response.getRemoteTcpInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int RemoteTCPInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    RemoteTCPInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureRemoteTCPInput *msg = MsgConfigureRemoteTCPInput::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteTCPInput *msgToGUI = MsgConfigureRemoteTCPInput::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void RemoteTCPInput::webapiUpdateDeviceSettings(
    RemoteTCPInputSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getRemoteTcpInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("loPpmCorrection")) {
        settings.m_loPpmCorrection = response.getRemoteTcpInputSettings()->getLoPpmCorrection();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getRemoteTcpInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getRemoteTcpInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getRemoteTcpInputSettings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("directSampling")) {
        settings.m_directSampling = response.getRemoteTcpInputSettings()->getDirectSampling() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getRemoteTcpInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getRemoteTcpInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("agc")) {
        settings.m_agc = response.getRemoteTcpInputSettings()->getAgc() != 0;
    }
    if (deviceSettingsKeys.contains("rfBW")) {
        settings.m_rfBW = response.getRemoteTcpInputSettings()->getRfBw();
    }
    if (deviceSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRemoteTcpInputSettings()->getInputFrequencyOffset();
    }
    if (deviceSettingsKeys.contains("channelGain")) {
        settings.m_channelGain = response.getRemoteTcpInputSettings()->getChannelGain();
    }
    if (deviceSettingsKeys.contains("channelSampleRate")) {
        settings.m_channelSampleRate = response.getRemoteTcpInputSettings()->getChannelSampleRate();
    }
    if (deviceSettingsKeys.contains("channelDecimation")) {
        settings.m_channelDecimation = response.getRemoteTcpInputSettings()->getChannelDecimation();
    }
    if (deviceSettingsKeys.contains("sampleBits")) {
        settings.m_sampleBits = response.getRemoteTcpInputSettings()->getSampleBits();
    }
    if (deviceSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteTcpInputSettings()->getDataAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getRemoteTcpInputSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("overrideRemoteSettings")) {
        settings.m_overrideRemoteSettings = response.getRemoteTcpInputSettings()->getOverrideRemoteSettings() != 0;
    }
    if (deviceSettingsKeys.contains("preFill")) {
        settings.m_preFill = response.getRemoteTcpInputSettings()->getPreFill() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteTcpInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteTcpInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteTcpInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteTcpInputSettings()->getReverseApiDeviceIndex();
    }
}

void RemoteTCPInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const RemoteTCPInputSettings& settings)
{
    response.getRemoteTcpInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getRemoteTcpInputSettings()->setLoPpmCorrection(settings.m_loPpmCorrection);
    response.getRemoteTcpInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getRemoteTcpInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getRemoteTcpInputSettings()->setBiasTee(settings.m_biasTee ? 1 : 0);
    response.getRemoteTcpInputSettings()->setDirectSampling(settings.m_directSampling ? 1 : 0);
    response.getRemoteTcpInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getRemoteTcpInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getRemoteTcpInputSettings()->setGain(settings.m_gain[0]);
    response.getRemoteTcpInputSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getRemoteTcpInputSettings()->setRfBw(settings.m_rfBW);
    response.getRemoteTcpInputSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRemoteTcpInputSettings()->setChannelGain(settings.m_channelGain);
    response.getRemoteTcpInputSettings()->setChannelSampleRate(settings.m_channelSampleRate);
    response.getRemoteTcpInputSettings()->setChannelDecimation(settings.m_channelDecimation);
    response.getRemoteTcpInputSettings()->setSampleBits(settings.m_sampleBits);
    response.getRemoteTcpInputSettings()->setDataAddress(new QString(settings.m_dataAddress));
    response.getRemoteTcpInputSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteTcpInputSettings()->setOverrideRemoteSettings(settings.m_overrideRemoteSettings ? 1 : 0);
    response.getRemoteTcpInputSettings()->setPreFill(settings.m_preFill ? 1 : 0);

    response.getRemoteTcpInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteTcpInputSettings()->getReverseApiAddress()) {
        *response.getRemoteTcpInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteTcpInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteTcpInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteTcpInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int RemoteTCPInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteTcpInputReport(new SWGSDRangel::SWGRemoteTCPInputReport());
    response.getRemoteTcpInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void RemoteTCPInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getRemoteTcpInputReport()->setSampleRate(m_settings.m_channelSampleRate);
}

void RemoteTCPInput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const RemoteTCPInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteTCPInput"));
    swgDeviceSettings->setRemoteTcpInputSettings(new SWGSDRangel::SWGRemoteTCPInputSettings());
    SWGSDRangel::SWGRemoteTCPInputSettings *swgRemoteTCPInputSettings = swgDeviceSettings->getRemoteTcpInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgRemoteTCPInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgRemoteTCPInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("biasTee") || force) {
        swgRemoteTCPInputSettings->setBiasTee(settings.m_biasTee ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dataAddress") || force) {
        swgRemoteTCPInputSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (deviceSettingsKeys.contains("dataPort") || force) {
        swgRemoteTCPInputSettings->setDataPort(settings.m_dataPort);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void RemoteTCPInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteTCPInput"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void RemoteTCPInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteTCPInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RemoteTCPInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
