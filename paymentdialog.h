/*
    This file is part of the Zero Reserve Plugin for Retroshare.

    Zero Reserve is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zero Reserve is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Zero Reserve.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PAYMENTDIALOG_H
#define PAYMENTDIALOG_H

#include "zrtypes.h"

#include <QDialog>
#include <QString>

#include <string>

namespace Ui {
    class PaymentDialog;
}

class PaymentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaymentDialog( const std::string & payee, QWidget *parent = 0 );
    ~PaymentDialog();

private slots:
    void payTo();
    void loadAvailableFunds( QString arg = "" );

private:
    ZR_Number availableFunds();

    Ui::PaymentDialog *ui;
    const std::string m_payee;
};

#endif // PAYMENTDIALOG_H
