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

#ifndef CASCRIPTEDITORDIALOG_H
#define CASCRIPTEDITORDIALOG_H

#include <QDialog>
#include <QPointer>
#include <qtcontrols_global.h>

class caScript;
class QLabel;
class QPlainTextEdit;
class QTimer;

class QTCON_EXPORT CaScriptEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CaScriptEditorDialog(caScript *widget, QWidget *parent = nullptr);

public slots:
    void accept() override;

private:
    QString source() const;
    bool checkSyntax();

    QPointer<caScript> m_widget;
    QPlainTextEdit *m_editor;
    QLabel *m_diagnostic;
    QTimer *m_checkTimer;
    QString m_initialText;
};

#endif // CASCRIPTEDITORDIALOG_H
