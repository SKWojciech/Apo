#pragma once
#include <QGraphicsView>
#include <QGraphicsItem>


////////////////////////
/// MyGraphicsView
/// Domyślnie QGraphicsView::fitInView dodaje margines i nie ma sposobu na wyłączenie tego >:(
/// Ten margines ma sens, jeśli krawędzie są wyokrąglone, ale ja chcę kwadratowe obramowanie!
/// Dodatkowo te marginesy powodują, że obraz się 'trzęsie' - pewnie przez błąd w zaokrągleniu
/// Więc muszę nadpisać kod bazowy QGraphicsView
////////////////////////

struct MyGraphicsView : QGraphicsView
{
    void fitInView(const QRectF& rect, Qt::AspectRatioMode aspectRatioMode) {
        if (scene() == nullptr || rect.isNull())
            return;
        resetTransform();

        QRectF viewRect = viewport()->rect();
        if (viewRect.isEmpty())
            return;
        QRectF sceneRect = this->sceneRect();
        if (sceneRect.isEmpty())
            return;
        qreal xratio = viewRect.width() / sceneRect.width();
        qreal yratio = viewRect.height() / sceneRect.height();

        switch (aspectRatioMode) {
        case Qt::KeepAspectRatio: xratio = yratio = qMin(xratio, yratio); break;
        case Qt::KeepAspectRatioByExpanding: xratio = yratio = qMax(xratio, yratio); break;
        case Qt::IgnoreAspectRatio: break;
        }

        scale(xratio, yratio);
        centerOn(rect.center());
    }
    void fitInView(qreal x, qreal y, qreal w, qreal h, Qt::AspectRatioMode aspectRatioMode) {
        fitInView(QRectF(x, y, w, h), aspectRatioMode);
    }
    void fitInView(const QGraphicsItem *item, Qt::AspectRatioMode aspectRatioMode) {
        if (item == nullptr)
            return;
        //QPainterPath path = item->isClipped() ? item->clipPath() : item->shape();
        fitInView(item->mapToScene(item->opaqueArea()).boundingRect(), aspectRatioMode);
    }
};
