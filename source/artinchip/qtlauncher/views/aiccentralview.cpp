#include "aiccentralview.h"
#include "utils/aicconsts.h"

AiCCentralView::AiCCentralView(QWidget *parent) : QWidget(parent)
{
    initView();
}

void AiCCentralView::initView()
{
    mGridLayout = new QGridLayout(this);
    mGridLayout->setContentsMargins(AIC_CENTRAL_BUTTON_XMARGIN,AIC_CENTRAL_BUTTON_YMARGIN,
                                    AIC_CENTRAL_BUTTON_XMARGIN,AIC_CENTRAL_BUTTON_YMARGIN);
    mGridLayout->setHorizontalSpacing(15);
    mGridLayout->setVerticalSpacing(5);
}

void AiCCentralView::addButtonWidget(QWidget * widget, int row, int column, int rowSpan, int columnSpan)
{
    mGridLayout->addWidget(widget, row, column, rowSpan, columnSpan);
}

AiCCentralView::~AiCCentralView()
{
    delete mGridLayout;
}
