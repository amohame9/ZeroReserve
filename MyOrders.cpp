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

#include "MyOrders.h"
#include "ZeroReservePlugin.h"
#include "p3ZeroReserverRS.h"
#include "Payment.h"
#include "TransactionManager.h"

#include <iostream>
#include <sstream>

MyOrders * MyOrders::me = NULL;

MyOrders * MyOrders::Instance(){ return me; }


MyOrders::MyOrders()
{
    me = this;
}

MyOrders::MyOrders( OrderBook * bids, OrderBook * asks ) :
    m_bids( bids ),
    m_asks( asks )
{
    m_bids->setMyOrders( this );
    m_asks->setMyOrders( this );

    me = this;
}

int MyOrders::columnCount(const QModelIndex&) const
{
    return 3;
}

QVariant MyOrders::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal) {
            switch (section)
            {
            case 0:
                return QString("Type");
            case 1:
                return QString("Volume");
            case 2:
                return QString("Price");
            }
        }
    }
    return QVariant();
}


QVariant MyOrders::data( const QModelIndex& index, int role ) const
{
    if (role == Qt::DisplayRole && index.row() < m_filteredOrders.size()){
        Order * order = m_filteredOrders[index.row()];
        switch(index.column())
        {
        case 0:
            if(order->m_orderType == Order::ASK )
                return QVariant( "Sell");
            else
                return QVariant( "Buy");
        case 1:
            return QVariant(order->m_amount);
        case 2:
            return QVariant(order->m_price);
        default:
            return QVariant();
        }
    }
    return QVariant();
}


int MyOrders::matchOther( Order * other )
{
    if( other->m_orderType == Order::BID ){
        return ZR::ZR_FAILURE; // TODO
    }
    p3ZeroReserveRS * p3zr = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
    if( other->m_trader_id == p3zr->getOwnId() ) return ZR::ZR_FAILURE; // don't fill own orders

    Order * order = NULL;
    for( OrderIterator it = m_orders.begin(); it != m_orders.end(); it++ ){
        order = *it;
        if( order->m_price < other->m_price ) break;    // no need to try and find matches beyond
        std::cerr << "Zero Reserve: Match at ask price " << order->m_price.toStdString() << std::endl;

        if( order->m_amount > other->m_amount ){
            buy( other, other->m_amount );
            order->m_amount = order->m_amount - other->m_amount;
        }
        else {
            buy( other, order->m_amount );
            order->m_amount = 0;
            // FIXME - wait until deal closed
            order->m_purpose = Order::FILLED;
            p3zr->publishOrder( order );
            return ZR::ZR_FINISH;
        }
    }
    // FIXME - wait until deal closed
    if( order ){
        order->m_purpose = Order::PARTLY_FILLED;
        p3zr->publishOrder( order );
    }
    return ZR::ZR_SUCCESS;
}


int MyOrders::match( Order * order )
{
    if( order->m_orderType == Order::ASK ){
        return ZR::ZR_FAILURE; // throw?
    }

    p3ZeroReserveRS * p3zr = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
    OrderList asks;
    m_asks->filterOrders( asks, order->m_currency );
    for( OrderIterator askIt = asks.begin(); askIt != asks.end(); askIt++ ){
        Order * other = *askIt;
        if( other->m_trader_id == p3zr->getOwnId() ) continue; // don't fill own orders
        if( order->m_price < other->m_price ) break;    // no need to try and find matches beyond
        std::cerr << "Zero Reserve: Match at ask price " << other->m_price.toStdString() << std::endl;

        if( order->m_amount > other->m_amount ){
            buy( other, other->m_amount );
            order->m_amount = order->m_amount - other->m_amount;
        }
        else {
            buy( other, order->m_amount );
            order->m_amount = 0;
            return ZR::ZR_FINISH;
        }
    }

    return ZR::ZR_SUCCESS;
}


void MyOrders::buy( Order * order, ZR::ZR_Number amount )
{
    TransactionManager * tm = new TransactionManager();
    ZR::ZR_Number amountToPay = amount * order->m_price;

    Payment * payment = new PaymentSpender( order->m_trader_id, amountToPay, Currency::currencySymbols[ order->m_currency ], Payment::BITCOIN );
    std::ostringstream timestamp;
    timestamp << order->m_timeStamp;
    payment->setText( timestamp.str() );
    if( ZR::ZR_FAILURE == tm->initCoordinator( payment ) ) delete tm;
}



int MyOrders::startExecute( Payment * payment )
{
    // TODO: start 2/3 Bitcoin TX here
    std::cerr << "Zero Reserve: Starting Order execution for " << payment->getText() << std::endl;
    ZR::RetVal result = ZR::ZR_FAILURE;
    p3ZeroReserveRS * p3zr = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );

    // find out if the payment really refers to an order of ours...
    Order order;
    std::stringstream s_timestamp( payment->getText() );
    s_timestamp >> order.m_timeStamp;
    order.m_trader_id = p3zr->getOwnId();
    order.m_currency = Currency::getCurrencyBySymbol( payment->getCurrency() );
    for( OrderIterator it = m_orders.begin(); it != m_orders.end(); it++ ){
        if( order == *(*it) ){
            // ... and if the amount to buy does not exceed the order.
            result = ( (*it)->m_price * (*it)->m_amount < payment->getAmount() )? ZR::ZR_FAILURE : ZR::ZR_SUCCESS;
            break;
        }
    }

    return result;
}


int MyOrders::finishExecute( Payment * payment )
{
    // TODO: sign 2/3 Bitcoin TX here
    std::cerr << "Zero Reserve: Finishing Order execution for " << payment->getText() << std::endl;
    p3ZeroReserveRS * p3zr = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
    Order order;
    std::stringstream s_timestamp( payment->getText() );
    s_timestamp >> order.m_timeStamp;
    order.m_trader_id = p3zr->getOwnId();
    order.m_currency = Currency::getCurrencyBySymbol( payment->getCurrency() );
    remove( &order );
    Order * oldOrder = m_asks->remove( &order );
    // TODO: publish delete order and possibly updated order
    if( NULL != oldOrder ){
        if( oldOrder->m_amount * oldOrder->m_price >  payment->getAmount() ){ // order only partly filled
            oldOrder->m_purpose = Order::PARTLY_FILLED;
            oldOrder->m_amount = oldOrder->m_amount - payment->getAmount() / oldOrder->m_price;
        }
        else {  // completely filled
            oldOrder->m_purpose = Order::FILLED;
        }
        p3zr->publishOrder( oldOrder );
    }
    else {
        std::cerr << "Zero Reserve: Could not find order" << std::endl;
        return ZR::ZR_FAILURE;
    }
    return ZR::ZR_SUCCESS;
}

void MyOrders::cancelOrder( int index )
{
    std::cerr << "Zero Reserve: Cancelling order: " << index << std::endl;
    p3ZeroReserveRS * p3zr = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
    Order * order = m_filteredOrders[ index ];

    remove( order );
    if( order->m_orderType == Order::ASK ){
        m_asks->remove( order );
    }
    else{
        m_bids->remove( order );
    }

    order->m_purpose = Order::CANCEL;
    p3zr->publishOrder( order );
}
