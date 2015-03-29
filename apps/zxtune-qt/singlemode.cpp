/**
*
* @file
*
* @brief Single instance support implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "app_parameters.h"
#include "singlemode.h"
#include "ui/utils.h"
//common includes
#include <contract.h>
//library includes
#include <debug/log.h>
//boost includes
#include <boost/scoped_ptr.hpp>
//qt includes
#include <QtCore/QDir>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
//text includes
#include "text/text.h"

namespace
{
  const Debug::Stream Dbg("SingleInstance");
}

namespace
{
  const QString SERVER_NAME(Text::PROGRAM_NAME);

  class StubModeDispatcher : public SingleModeDispatcher
  {
  public:
    StubModeDispatcher(int argc, const char* argv[])
    {
      QDir curDir;
      for (int i = 0; i < argc; ++i)
      {
        const QString arg(argv[i]);
        if (arg.startsWith("--"))
        {
          Cmdline << arg;
        }
        else
        {
          Cmdline << curDir.cleanPath(curDir.absoluteFilePath(arg));
        }
      }
    }

    virtual bool StartMaster()
    {
      return true;
    }

    virtual QStringList GetCmdline() const
    {
      return Cmdline;
    }

  protected:
    virtual void SlaveStarted() {}
  protected:
    QStringList Cmdline;
  };

  class SocketBasedSingleModeDispatcher : public StubModeDispatcher
  {
  public:
    SocketBasedSingleModeDispatcher(int argc, const char* argv[])
      : StubModeDispatcher(argc, argv)
    {
    }

    virtual bool StartMaster()
    {
      QLocalSocket socket;
      socket.connectToServer(SERVER_NAME, QLocalSocket::WriteOnly);
      if (socket.waitForConnected(500))
      {
        Dbg("Connected to existing server. Sending cmdline with %1% args", Cmdline.size());
        SendDataTo(socket);
        return false;
      }
      else
      {
        StartLocalServer();
        return true;
      }
    }

  private:
    virtual void SlaveStarted()
    {
      while (QLocalSocket* conn = Server->nextPendingConnection())
      {
        const boost::scoped_ptr<QLocalSocket> holder(conn);
        QStringList cmdline;
        ReadDataFrom(*holder, cmdline);
        if (cmdline.empty())
        {
          Dbg("Empty cmdline from slave. Ignored");
        }
        else
        {
          Dbg("Slave passed cmdline '%1%'", FromQString(cmdline.join(" ")));
          emit OnSlaveStarted(cmdline);
        }
      }
    }
  private:
    class SocketHelper
    {
    public:
      explicit SocketHelper(QLocalSocket& sock)
        : Sock(sock)
      {
      }

      void Write(const QByteArray& data)
      {
        const quint32 size = data.size();
        Write(&size, sizeof(size));
        Write(data.data(), size);
      }

      void Read(QByteArray& data)
      {
        quint32 size = 0;
        Read(&size, sizeof(size));
        data.resize(size);
        Read(data.data(), size);
      }
    private:
      void Write(const void* data, quint64 size)
      {
        const char* ptr = static_cast<const char*>(data);
        quint64 rest = size;
        while (rest != 0)
        {
          if (const quint64 written = Sock.write(ptr, rest))
          {
            ptr += written;
            rest -= written;
          }
          else
          {
            Sock.waitForBytesWritten();
          }
        }
        Sock.waitForBytesWritten();
      }

      void Read(void* data, quint64 size)
      {
        char* ptr = static_cast<char*>(data);
        quint64 rest = size;
        while (rest != 0)
        {
          if (const quint64 read = Sock.read(ptr, rest))
          {
            ptr += read;
            rest -= read;
          }
          else
          {
            Sock.waitForReadyRead();
          }
        }
      }

    private:
      QLocalSocket& Sock;
    };

    void SendDataTo(QLocalSocket& socket)
    {
      QByteArray blob;
      QDataStream stream(&blob, QIODevice::WriteOnly);
      stream.setVersion(QDataStream::Qt_4_8);
      stream << Cmdline;
      stream.device()->seek(0);
      SocketHelper out(socket);
      out.Write(blob);
    }

    static void ReadDataFrom(QLocalSocket& socket, QStringList& result)
    {
      SocketHelper in(socket);
      QByteArray blob;
      in.Read(blob);
      QDataStream stream(&blob, QIODevice::ReadOnly);
      stream.setVersion(QDataStream::Qt_4_8);
      stream >> result;
    }

    void StartLocalServer()
    {
      Server.reset(new QLocalServer(this));
      Require(connect(Server.get(), SIGNAL(newConnection()), SLOT(SlaveStarted())));
      while (!Server->listen(SERVER_NAME))
      {
        if (Server->serverError() == QAbstractSocket::AddressInUseError
         && QLocalServer::removeServer(SERVER_NAME))
        {
          Dbg("Try to restore from previously crashed session");
          continue;
        }
        else
        {
          Dbg("Failed to start local server");
          break;
        }
      }
    }

  private:
    std::auto_ptr<QLocalServer> Server;
  };
}

SingleModeDispatcher::Ptr SingleModeDispatcher::Create(Parameters::Accessor::Ptr params, int argc, const char *argv[])
{
  Parameters::IntType val = Parameters::ZXTuneQT::SINGLE_INSTANCE_DEFAULT;
  params->FindValue(Parameters::ZXTuneQT::SINGLE_INSTANCE, val);
  if (val != 0)
  {
    Dbg("Working in single instance mode");
    return SingleModeDispatcher::Ptr(new SocketBasedSingleModeDispatcher(argc, argv));
  }
  else
  {
    Dbg("Working in multiple instances mode");
    return SingleModeDispatcher::Ptr(new StubModeDispatcher(argc, argv));
  }
}
