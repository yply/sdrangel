///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/datafifo.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/devicesamplesource.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "maincore.h"

#include "demodanalyzerworker.h"
#include "demodanalyzer.h"

MESSAGE_CLASS_DEFINITION(DemodAnalyzer::MsgConfigureDemodAnalyzer, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzer::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzer::MsgRefreshChannels, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzer::MsgReportChannels, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzer::MsgSelectChannel, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzer::MsgReportSampleRate, Message)

const char* const DemodAnalyzer::m_featureIdURI = "sdrangel.feature.demodanalyzer";
const char* const DemodAnalyzer::m_featureId = "DemodAnalyzer";

DemodAnalyzer::DemodAnalyzer(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_spectrumVis(SDR_RX_SCALEF),
    m_selectedChannel(nullptr),
    m_dataPipe(nullptr)
{
    qDebug("DemodAnalyzer::DemodAnalyzer: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_worker = new DemodAnalyzerWorker();
    m_worker->moveToThread(&m_thread);
    m_worker->setScopeVis(&m_scopeVis);
    m_state = StIdle;
    m_errorMessage = "DemodAnalyzer error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DemodAnalyzer::networkManagerFinished
    );
}

DemodAnalyzer::~DemodAnalyzer()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DemodAnalyzer::networkManagerFinished
    );
    delete m_networkManager;
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
}

void DemodAnalyzer::start()
{
	qDebug("DemodAnalyzer::start");

    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->reset();
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    DemodAnalyzerWorker::MsgConfigureDemodAnalyzerWorker *msg
        = DemodAnalyzerWorker::MsgConfigureDemodAnalyzerWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo)
        {
            DemodAnalyzerWorker::MsgConnectFifo *msg = DemodAnalyzerWorker::MsgConnectFifo::create(fifo, true);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }
}

void DemodAnalyzer::stop()
{
    qDebug("DemodAnalyzer::stop");

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo)
        {
            DemodAnalyzerWorker::MsgConnectFifo *msg = DemodAnalyzerWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

	m_worker->stopWork();
    m_state = StIdle;
	m_thread.quit();
	m_thread.wait();
}

double DemodAnalyzer::getMagSqAvg() const
{
    return m_worker->getMagSqAvg();
}

bool DemodAnalyzer::handleMessage(const Message& cmd)
{
	if (MsgConfigureDemodAnalyzer::match(cmd))
	{
        MsgConfigureDemodAnalyzer& cfg = (MsgConfigureDemodAnalyzer&) cmd;
        qDebug() << "DemodAnalyzer::handleMessage: MsgConfigureDemodAnalyzer";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "DemodAnalyzer::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgRefreshChannels::match(cmd))
    {
        qDebug() << "DemodAnalyzer::handleMessage: MsgRefreshChannels";
        updateChannels();
        return true;
    }
    else if (MsgSelectChannel::match(cmd))
    {
        MsgSelectChannel& cfg = (MsgSelectChannel&) cmd;
        ChannelAPI *selectedChannel = cfg.getChannel();
        qDebug("DemodAnalyzer::handleMessage: MsgSelectChannel: %p %s",
            selectedChannel, qPrintable(selectedChannel->objectName()));
        setChannel(selectedChannel);
        MainCore::MsgChannelDemodQuery *msg = MainCore::MsgChannelDemodQuery::create();
        selectedChannel->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MainCore::MsgChannelDemodReport::match(cmd))
    {
        qDebug() << "DemodAnalyzer::handleMessage: MainCore::MsgChannelDemodReport";
        MainCore::MsgChannelDemodReport& report = (MainCore::MsgChannelDemodReport&) cmd;

        if (report.getChannelAPI() == m_selectedChannel)
        {
            m_sampleRate = report.getSampleRate();
            m_scopeVis.setLiveRate(m_sampleRate);

            DSPSignalNotification *msg = new DSPSignalNotification(0, m_sampleRate);
            m_spectrumVis.getInputMessageQueue()->push(msg);

            if (m_dataPipe)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

                if (fifo) {
                    fifo->setSize(2*m_sampleRate);
                }
            }

            if (getMessageQueueToGUI())
            {
                MsgReportSampleRate *msg = MsgReportSampleRate::create(m_sampleRate);
                getMessageQueueToGUI()->push(msg);
            }
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray DemodAnalyzer::serialize() const
{
    return m_settings.serialize();
}

bool DemodAnalyzer::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDemodAnalyzer *msg = MsgConfigureDemodAnalyzer::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDemodAnalyzer *msg = MsgConfigureDemodAnalyzer::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DemodAnalyzer::applySettings(const DemodAnalyzerSettings& settings, bool force)
{
    qDebug() << "DemodAnalyzer::applySettings:"
            << " m_log2Decim: " << settings.m_log2Decim
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIFeatureSetIndex: " << settings.m_reverseAPIFeatureSetIndex
            << " m_reverseAPIFeatureIndex: " << settings.m_reverseAPIFeatureIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }

    DemodAnalyzerWorker::MsgConfigureDemodAnalyzerWorker *msg = DemodAnalyzerWorker::MsgConfigureDemodAnalyzerWorker::create(
        settings, force
    );
    m_worker->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIFeatureSetIndex != settings.m_reverseAPIFeatureSetIndex) ||
                (m_settings.m_reverseAPIFeatureIndex != settings.m_reverseAPIFeatureIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

void DemodAnalyzer::updateChannels()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();
    m_availableChannels.clear();

    int deviceIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine =  (*it)->m_deviceSinkEngine;

        if (deviceSourceEngine || deviceSinkEngine)
        {
            for (int chi = 0; chi < (*it)->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = (*it)->getChannelAt(chi);
                int i = DemodAnalyzerSettings::m_channelURIs.indexOf(channel->getURI());

                if (i >= 0)
                {
                    DemodAnalyzerSettings::AvailableChannel availableChannel =
                        DemodAnalyzerSettings::AvailableChannel{
                            deviceSinkEngine != nullptr,
                            deviceIndex,
                            chi,
                            channel,
                            DemodAnalyzerSettings::m_channelTypes.at(i)
                        };
                    m_availableChannels[channel] = availableChannel;
                }
            }
        }
    }

    if (getMessageQueueToGUI())
    {
        MsgReportChannels *msgToGUI = MsgReportChannels::create();
        QList<DemodAnalyzerSettings::AvailableChannel>& msgAvailableChannels = msgToGUI->getAvailableChannels();
        QHash<ChannelAPI*, DemodAnalyzerSettings::AvailableChannel>::iterator it = m_availableChannels.begin();

        for (; it != m_availableChannels.end(); ++it) {
            msgAvailableChannels.push_back(*it);
        }

        getMessageQueueToGUI()->push(msgToGUI);
    }
}

void DemodAnalyzer::setChannel(ChannelAPI *selectedChannel)
{
    if ((selectedChannel == m_selectedChannel) || !m_availableChannels.contains(selectedChannel)) {
        return;
    }

    MainCore *mainCore = MainCore::instance();

    if (m_selectedChannel)
    {
        ObjectPipe *pipe = mainCore->getDataPipes().unregisterProducerToConsumer(m_selectedChannel, this, "demod");
        DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element);

        if ((fifo) && m_worker->isRunning())
        {
            DemodAnalyzerWorker::MsgConnectFifo *msg = DemodAnalyzerWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }

        ObjectPipe *messagePipe = mainCore->getMessagePipes().unregisterProducerToConsumer(m_selectedChannel, this, "reportdemod");

        if (messagePipe)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(messagePipe->m_element);

            if (messageQueue) {
                disconnect(messageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleChannelMessageQueue(MessageQueue*)));
            }
        }
    }

    m_dataPipe = mainCore->getDataPipes().registerProducerToConsumer(selectedChannel, this, "demod");
    connect(m_dataPipe, SIGNAL(toBeDeleted(int, QObject*)), this, SLOT(handleDataPipeToBeDeleted(int, QObject*)));
    DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

    if (fifo)
    {
        fifo->setSize(96000);

        if (m_worker->isRunning())
        {
            DemodAnalyzerWorker::MsgConnectFifo *msg = DemodAnalyzerWorker::MsgConnectFifo::create(fifo, true);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

    ObjectPipe *messagePipe = mainCore->getMessagePipes().registerProducerToConsumer(selectedChannel, this, "reportdemod");

    if (messagePipe)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(messagePipe->m_element);

        if (messageQueue)
        {
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
        }
    }

    m_selectedChannel = selectedChannel;
}

int DemodAnalyzer::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int DemodAnalyzer::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setDemodAnalyzerSettings(new SWGSDRangel::SWGDemodAnalyzerSettings());
    response.getDemodAnalyzerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int DemodAnalyzer::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    DemodAnalyzerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureDemodAnalyzer *msg = MsgConfigureDemodAnalyzer::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DemodAnalyzer::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDemodAnalyzer *msgToGUI = MsgConfigureDemodAnalyzer::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void DemodAnalyzer::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const DemodAnalyzerSettings& settings)
{
    if (response.getDemodAnalyzerSettings()->getTitle()) {
        *response.getDemodAnalyzerSettings()->getTitle() = settings.m_title;
    } else {
        response.getDemodAnalyzerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getDemodAnalyzerSettings()->setLog2Decim(settings.m_log2Decim);
    response.getDemodAnalyzerSettings()->setRgbColor(settings.m_rgbColor);
    response.getDemodAnalyzerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDemodAnalyzerSettings()->getReverseApiAddress()) {
        *response.getDemodAnalyzerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDemodAnalyzerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDemodAnalyzerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDemodAnalyzerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getDemodAnalyzerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getDemodAnalyzerSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getDemodAnalyzerSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getDemodAnalyzerSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_scopeGUI)
    {
        if (response.getDemodAnalyzerSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getDemodAnalyzerSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getDemodAnalyzerSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getDemodAnalyzerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDemodAnalyzerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDemodAnalyzerSettings()->setRollupState(swgRollupState);
        }
    }
}

void DemodAnalyzer::webapiUpdateFeatureSettings(
    DemodAnalyzerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getDemodAnalyzerSettings()->getLog2Decim();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getDemodAnalyzerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDemodAnalyzerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDemodAnalyzerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDemodAnalyzerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDemodAnalyzerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getDemodAnalyzerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getDemodAnalyzerSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_spectrumGUI && featureSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(featureSettingsKeys, response.getDemodAnalyzerSettings()->getSpectrumConfig());
    }
    if (settings.m_scopeGUI && featureSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(featureSettingsKeys, response.getDemodAnalyzerSettings()->getScopeConfig());
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getDemodAnalyzerSettings()->getRollupState());
    }
}

void DemodAnalyzer::webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const DemodAnalyzerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("DemodAnalyzer"));
    swgFeatureSettings->setDemodAnalyzerSettings(new SWGSDRangel::SWGDemodAnalyzerSettings());
    SWGSDRangel::SWGDemodAnalyzerSettings *swgDemodAnalyzerSettings = swgFeatureSettings->getDemodAnalyzerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("log2Decim") || force) {
        swgDemodAnalyzerSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgDemodAnalyzerSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgDemodAnalyzerSettings->setRgbColor(settings.m_rgbColor);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void DemodAnalyzer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DemodAnalyzer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DemodAnalyzer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void DemodAnalyzer::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void DemodAnalyzer::handleDataPipeToBeDeleted(int reason, QObject *object)
{
    qDebug("DemodAnalyzer::handleDataPipeToBeDeleted: %d %p", reason, object);

    if ((reason == 0) && (m_selectedChannel == object))
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo && m_worker->isRunning())
        {
            DemodAnalyzerWorker::MsgConnectFifo *msg = DemodAnalyzerWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }

        updateChannels();
        m_selectedChannel = nullptr;
    }
}
