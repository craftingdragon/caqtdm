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

#include "cascript.h"

#include <QSet>

caScript::caScript(QWidget *parent) : QWidget(parent)
{}

QString caScript::getScript() const
{
    return m_script;
}

void caScript::setScript(const QString &script)
{
    m_script = script;
}

QStringList caScript::validateInputChannels(const QList<InputChannel> &inputs)
{
    QStringList errors;
    QSet<QString> keys;

    for (const InputChannel &input : inputs) {
        const QString key = input.alias.isEmpty() ? input.channel : input.alias;
        if (keys.contains(key)) {
            errors.append(tr("Duplicate input key \"%1\".").arg(key));
        } else {
            keys.insert(key);
        }
    }

    return errors;
}
