#include "controllers/controllerenumerator.h"

#include <utility>

#include "controllers/controller.h"
#include "moc_controllerenumerator.cpp"

ControllerEnumerator::ControllerEnumerator() = default;

ControllerEnumerator::~ControllerEnumerator() {
    // Whatever nobody took.
    qDeleteAll(m_retiredDevices);
}

QList<Controller*> ControllerEnumerator::takeRetiredDevices() {
    return std::exchange(m_retiredDevices, {});
}

void ControllerEnumerator::retireDevices(QList<Controller*>* pDevices) {
    for (Controller* pController : std::as_const(*pDevices)) {
        if (pController->isOpen()) {
            pController->close();
        }
    }
    m_retiredDevices.append(*pDevices);
    pDevices->clear();
}
