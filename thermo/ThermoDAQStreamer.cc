#include <iostream>

#include <QDateTime>

#include "ThermoDAQStreamer.h"
#include "ThermoDAQModel.h"

ThermoDAQStreamer::ThermoDAQStreamer(ThermoDAQModel* model, QObject* parent) :
    QObject(parent),
    model_(model),
    isStreaming_(false),
    ofile_(0),
    stream_(0)
{
    connect(model_, SIGNAL(daqStateChanged(bool)),
            this, SLOT(daqStateChanged(bool)));
    connect(model_, SIGNAL(daqMessage(QString)),
            this, SLOT(handleDAQMessage(QString)));
}

void ThermoDAQStreamer::handleDAQMessage(const QString& message)
{
    std::cout << message.toStdString() << std::endl;
    if (isStreaming_) {
        *stream_ << message << "\n";
        stream_->flush();
    }
}

void ThermoDAQStreamer::daqStateChanged(bool state)
{
    if (state==true) {
        QDateTime dt = QDateTime::currentDateTimeUtc();

        QString measurementDirPath(QDir::homePath() + "/Desktop/measurements/%1");
        currentDir_.setPath(measurementDirPath.arg(dt.toString("yyyyMMdd")));
        if (!currentDir_.exists())
            currentDir_.mkpath(currentDir_.absolutePath());

        QString filename("%1-%2.xml");
        filename = filename.arg(dt.toString("yyyyMMdd"));
        int i = 1;
        while (currentDir_.exists(filename.arg(i))) ++i;
        filename = filename.arg(i);

        ofile_ = new QFile(currentDir_.absoluteFilePath(filename));
        if (ofile_->open(QFile::WriteOnly | QFile::Truncate)) {
            stream_ = new QTextStream(ofile_);
        }
    } else {
        stream_->device()->close();
        delete ofile_;
        delete stream_;
    }

    isStreaming_ = state;
}