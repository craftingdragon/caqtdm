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

#include "cascriptdialog.h"
#include "cascript.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QDesignerFormWindowInterface>
#include <QDesignerFormWindowCursorInterface>
#include <QTableWidget>
#include <QVBoxLayout>

CaScriptDialog::CaScriptDialog(caScript *widget, const QString &property, QWidget *parent)
    : QDialog(parent), m_widget(widget), m_property(property)
{
    const bool outputs = property == QStringLiteral("outputChannels");
    setWindowTitle(outputs ? tr("Edit output channels") : tr("Edit input channels"));
    const auto channels = outputs ? widget->getOutputChannels() : widget->getInputChannels();

    auto *layout = new QVBoxLayout(this);
    auto *table = new QTableWidget(channels.size(), 2, this);
    m_table = table;
    table->setHorizontalHeaderLabels({tr("Channel"), tr("Alias (optional)")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    for (int row = 0; row < channels.size(); ++row) {
        table->setItem(row, 0, new QTableWidgetItem(channels.at(row).channel));
        table->setItem(row, 1, new QTableWidgetItem(channels.at(row).alias));
    }
    layout->addWidget(table);

    auto *rowActions = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add"), this);
    auto *remove = new QPushButton(tr("Remove"), this);
    add->setAutoDefault(false);
    remove->setAutoDefault(false);
    add->setObjectName(QStringLiteral("addChannel"));
    remove->setObjectName(QStringLiteral("removeChannel"));
    rowActions->addWidget(add);
    rowActions->addWidget(remove);
    rowActions->addStretch();
    layout->addLayout(rowActions);
    connect(add, &QPushButton::clicked, table, [table]() {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem);
        table->setItem(row, 1, new QTableWidgetItem);
        table->setCurrentCell(row, 0);
        table->setFocus();
    });
    connect(remove, &QPushButton::clicked, table, [table]() {
        const auto rows = table->selectionModel()->selectedRows();
        if (!rows.isEmpty()) table->removeRow(rows.first().row());
    });

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("configurationError"));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);
    connect(table, &QTableWidget::itemChanged, m_error, &QWidget::hide);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CaScriptDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(600, 300);
}

void CaScriptDialog::accept()
{
    QList<caScript::InputChannel> channels;
    QJsonArray entries;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *channelItem = m_table->item(row, 0);
        const QTableWidgetItem *aliasItem = m_table->item(row, 1);
        const QString channel = channelItem ? channelItem->text().trimmed() : QString();
        const QString alias = aliasItem ? aliasItem->text().trimmed() : QString();
        channels.append({channel, alias});
        entries.append(QJsonObject{{QStringLiteral("channel"), channel}, {QStringLiteral("alias"), alias}});
    }
    const QStringList errors = caScript::validateInputChannels(channels);
    if (!errors.isEmpty()) {
        m_error->setText(errors.join('\n'));
        m_error->show();
        return;
    }
    if (QDesignerFormWindowInterface *form = QDesignerFormWindowInterface::findFormWindow(m_widget)) {
        form->cursor()->setProperty(m_property,
            QString::fromUtf8(QJsonDocument(entries).toJson(QJsonDocument::Compact)));
        QDialog::accept();
    } else {
        m_error->setText(tr("This editor requires a Qt Designer form."));
        m_error->show();
    }
}
