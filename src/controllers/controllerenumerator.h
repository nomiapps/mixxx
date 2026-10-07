#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

class Controller;

/// Base class handling discovery and enumeration of DJ controllers.
///
/// This class handles discovery and enumeration of DJ controllers and
/// must be inherited by a class that implements it on some API.
class ControllerEnumerator : public QObject {
    Q_OBJECT
  public:
    ControllerEnumerator();
    // In this function, the inheriting class must delete the Controllers it
    // creates
    virtual ~ControllerEnumerator();

    /// Builds the list of controllers afresh. The controllers of the previous
    /// call are closed and handed to takeRetiredDevices(), not deleted: other
    /// threads may still hold pointers to them.
    virtual QList<Controller*> queryDevices() = 0;

    /// The controllers queryDevices() has replaced since the last call. They
    /// are closed, and the caller now owns them: it deletes them once nothing
    /// refers to them any more.
    QList<Controller*> takeRetiredDevices();

    /// The devices this enumerator would offer right now, keyed by an id that
    /// stays the same while a device stays plugged in, and mapped to the name its
    /// Controller would get. Read without touching any Controller, so it can run
    /// whenever the OS reports a device change. An enumerator that cannot tell
    /// returns nothing; its devices are then picked up only by a rescan.
    virtual QMap<QString, QString> presentDevices() const {
        return {};
    }

  protected:
    /// For queryDevices(): closes every controller in the list, keeps them
    /// for takeRetiredDevices(), and empties the list.
    void retireDevices(QList<Controller*>* pDevices);

  private:
    QList<Controller*> m_retiredDevices;
};
