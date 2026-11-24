/*
 *  This file is part of the caQtDM Framework, developed at the Paul Scherrer Institut,
 *  Villigen, Switzerland
 *
 *  The caQtDM Framework is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The caQtDM Framework is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the caQtDM Framework.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (c) 2026
 *
 *  Author:
 *    Julian Houba
 *  Contact details:
 *    julian.houba@psi.ch
 */

#include "cascripttaskmenu.h"
#include "cascriptdialog.h"
#include "cascripteditordialog.h"
#include "cascript.h"

CaScriptTaskMenu::CaScriptTaskMenu(caScript *widget, QObject *parent) : QObject(parent)
{
    m_editScript = new QAction(tr("Edit JavaScript..."), this);
    m_inputs = new QAction(tr("Edit input channels..."), this);
    m_outputs = new QAction(tr("Edit output channels..."), this);
    connect(m_editScript, &QAction::triggered, this, [widget]() {
        CaScriptEditorDialog dialog(widget, widget->window());
        dialog.exec();
    });
    connect(m_inputs, &QAction::triggered, this, [widget]() {
        CaScriptDialog dialog(widget, QStringLiteral("channels"), widget->window());
        dialog.exec();
    });
    connect(m_outputs, &QAction::triggered, this, [widget]() {
        CaScriptDialog dialog(widget, QStringLiteral("outputChannels"), widget->window());
        dialog.exec();
    });
}

QAction *CaScriptTaskMenu::preferredEditAction() const
{
    return m_editScript;
}

QList<QAction *> CaScriptTaskMenu::taskActions() const
{
    return {m_editScript, m_inputs, m_outputs};
}

CaScriptTaskMenuFactory::CaScriptTaskMenuFactory(QExtensionManager *parent) : QExtensionFactory(parent)
{
}

QObject *CaScriptTaskMenuFactory::createExtension(QObject *object, const QString &iid, QObject *parent) const
{
    if (iid != Q_TYPEID(QDesignerTaskMenuExtension)) return nullptr;
    if (caScript *widget = qobject_cast<caScript *>(object)) {
        return new CaScriptTaskMenu(widget, parent);
    }
    return nullptr;
}
