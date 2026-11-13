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
#include <QTableWidget>
#include <QVBoxLayout>

CaScriptDialog::CaScriptDialog(caScript *widget, const QString &property, QWidget *parent)
    : QDialog(parent)
{
    const bool outputs = property == QStringLiteral("outputChannels");
    setWindowTitle(outputs ? tr("Edit output channels") : tr("Edit input channels"));
    const auto channels = outputs ? widget->getOutputChannels() : widget->getInputChannels();

    auto *layout = new QVBoxLayout(this);
    auto *table = new QTableWidget(channels.size(), 2, this);
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

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(600, 300);
}
