// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "avatarlistwidget.h"

#include "src/plugin-accounts/operation/user.h"
#include "widgets/buttontuple.h"

#include <DConfig>
#include <DDialog>
#include <DDialogCloseButton>
#include <DStyle>

#include <QDebug>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPainterPath>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE
using namespace DCC_NAMESPACE;

AvatarListDialog::AvatarListDialog(User *usr)
    : m_curUser(usr)
    , m_mainContentLayout(new QHBoxLayout)
    , m_leftContentLayout(new QVBoxLayout)
    , m_rightContentLayout(new QVBoxLayout)
    , m_avatarSelectItem(new DListView(this))
    , m_avatarSelectItemModel(new QStandardItemModel(this))
    , m_avatarArea(new QScrollArea(this))
{
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    setWindowFlags(Qt::FramelessWindowHint);

    m_mainContentLayout->setContentsMargins(0, 0, 0, 0);
    m_rightContentLayout->setContentsMargins(0, 0, 0, 0);

    // 窗口Icon
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(qApp->windowIcon().pixmap(QSize(40, 40)));

    // 窗口关闭按钮
    auto closeBtn = new DDialogCloseButton(this);
    closeBtn->setIcon(DStyle().standardIcon(DStyle::SP_DialogCloseButton));
    closeBtn->setIconSize(QSize(30, 30));
    QHBoxLayout *closeBtnLayout = new QHBoxLayout;
    closeBtnLayout->setContentsMargins(0, 0, 0, 10);
    closeBtnLayout->addStretch();
    closeBtnLayout->addWidget(closeBtn);

    connect(closeBtn, &QPushButton::clicked, this, &AvatarListDialog::close);

    m_rightContentLayout->addLayout(closeBtnLayout);

    QList<AvatarItem> items = {
        AvatarItem(tr("Person"), "dcc_user_human", Role::Person, true),
        AvatarItem(tr("Animal"), "dcc_user_animal", Role::Animal, true),
        // 图片未提供, 先不加载
        AvatarItem(tr("Illustration"), "dcc_user_animal", Role::Illustration, false),
        AvatarItem(tr("Expression"), "dcc_user_emoji", Role::Expression, true),
        AvatarItem(tr("Custom Picture"), "dcc_user_custom", Role::Custom, true),
    };

    for (const auto &item : items) {
        if (item.isLoader) {
            DStandardItem *avatarItem = new DStandardItem(item.name);
            avatarItem->setFontSize(DFontSizeManager::SizeType::T5);
            avatarItem->setIcon(QIcon::fromTheme(item.icon));
            avatarItem->setData(item.role, AvatarItemNameRole);
            m_avatarSelectItemModel->appendRow(avatarItem);

            if (item.role == Role::Custom) {
                m_avatarFrames[AvatarAdd] = new CustomAddAvatarWidget(m_curUser, Role::Custom, this);
                m_avatarFrames[Role::Custom] = new CustomAvatarWidget(m_curUser, Role::Custom, this);
            } else {
                m_avatarFrames[item.role] = new AvatarListFrame(m_curUser, item.role, this);
            }
        }
    }

    // 添加选择Item
    m_avatarSelectItem->setModel(m_avatarSelectItemModel);
    m_avatarSelectItem->setAccessibleName("List_AvatarSelect");
    m_avatarSelectItem->setFrameShape(QFrame::NoFrame);
    m_avatarSelectItem->setItemSpacing(2);
    m_avatarSelectItem->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_avatarSelectItem->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_avatarSelectItem->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_leftContentLayout->setContentsMargins(10, 0, 0, 0);
    m_leftContentLayout->addWidget(iconLabel);
    m_leftContentLayout->addSpacing(12);
    m_leftContentLayout->addWidget(m_avatarSelectItem);

    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 10, 0, 10);
    hLayout->addLayout(m_leftContentLayout);

    QFrame *avarSelectWidget = new QFrame(this);
    avarSelectWidget->setFixedSize(180, 472);
    avarSelectWidget->setLayout(hLayout);

    m_mainContentLayout->addWidget(avarSelectWidget);

    QStackedWidget *avatarSelectWidget = new QStackedWidget(this);
    avatarSelectWidget->setFixedWidth(450);

    for (auto iter = m_avatarFrames.begin(); iter != m_avatarFrames.end(); ++iter) {
        avatarSelectWidget->addWidget(iter.value());

        auto listView = iter.value()->getCurrentListView();
        if (listView && listView->getCurrentListViewRole() != Role::AvatarAdd) {
            connect(listView,
                    &AvatarListView::requestUpdateListView,
                    this,
                    [this](bool isNeedSave, const auto &role, const auto &type) {
                        Q_UNUSED(type);

                        for (auto it = m_avatarFrames.begin(); it != m_avatarFrames.end(); ++it) {
                            auto frame = it.value();

                            if (frame->getCurrentRole() != role) {
                                if (frame->getCurrentListView()) {
                                    frame->getCurrentListView()->setCurrentAvatarUnChecked();
                                }
                            }
                        }

                        if (role == Custom) {
                            // 如果是新添加进来的用户头像, 先保存, 然后再更新用户头像编辑界面
                            if (isNeedSave) {
                                Q_EMIT requestSaveAvatar(m_avatarFrames[role]
                                                                 ->getCurrentListView()
                                                                 ->getAvatarPath());

                                connect(m_curUser,
                                        &User::currentAvatarChanged,
                                        this,
                                        [this](const QString &path) {
                                            if (path.contains(
                                                        m_avatarFrames[Custom]->getCurrentPath())) {
                                                getCustomAvatarWidget()
                                                        ->getCurrentListView()
                                                        ->requestUpdateCustomAvatar(path);
                                                getCustomAvatarWidget()
                                                        ->getCustomAvatarView()
                                                        ->setAvatarPath(
                                                                m_avatarFrames[Custom]
                                                                        ->getCurrentListView()
                                                                        ->getAvatarPath());
                                            }
                                        });

                                return;
                            }

                            getCustomAvatarWidget()->getCustomAvatarView()->setAvatarPath(
                                    m_avatarFrames[role]->getCurrentListView()->getAvatarPath());
                        }
                    });
        }
    }

    m_currentSelectAvatarWidget = m_avatarFrames[Person];

    connect(m_avatarSelectItem, &DListView::clicked, this, [this, avatarSelectWidget](auto &index) {
        // 如果没有添加自定义头像, 显示自定义添加图像页面
        if (!m_avatarFrames[Custom]->isExistCustomAvatar(
                    m_avatarFrames[Custom]->getCurrentPath(), m_curUser->name())) {
            if (index.row() == 3) {
                avatarSelectWidget->setCurrentIndex(index.row() + 1);
                m_currentSelectAvatarWidget = m_avatarFrames[Custom];

                return;
            }
        }

        // 切换到自定义头像界面, 更新用户头像编辑页面
        if (index.row() == 3) {
            getCustomAvatarWidget()->getCustomAvatarView()->setAvatarPath(
                    m_avatarFrames[Custom]->getCurrentListView()->getAvatarPath());
        }

        avatarSelectWidget->setCurrentIndex(index.row());
        m_currentSelectAvatarWidget =
                static_cast<AvatarListFrame *>(avatarSelectWidget->currentWidget());
    });

    QHBoxLayout *avatarLayout = new QHBoxLayout();
    avatarLayout->setContentsMargins(0, 0, 0, 0);
    m_avatarArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_avatarArea->setWidgetResizable(false);
    m_avatarArea->setFrameShape(QFrame::NoFrame);
    m_avatarArea->setWidget(avatarSelectWidget);
    avatarLayout->addWidget(m_avatarArea, Qt::AlignCenter);
    m_rightContentLayout->addLayout(avatarLayout);

    // 添加（关闭，保存）按钮
    auto buttonTuple = new ButtonTuple(ButtonTuple::Save, this);
    auto cancelButton = buttonTuple->leftButton();
    cancelButton->setText(tr("Cancel"));
    auto saveButton = buttonTuple->rightButton();
    saveButton->setText(tr("Save"));
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(10, 10, 10, 10);
    btnLayout->addWidget(cancelButton);
    btnLayout->addSpacing(10);
    btnLayout->addWidget(saveButton);

    connect(getCustomAvatarWidget()->getCustomAvatarView(),
            &CustomAvatarView::requestSaveCustomAvatar,
            this,
            [this](const QString &path) {
                if (!path.isEmpty()) {
                    m_currentSelectAvatarWidget->getCurrentListView()->saveAvatar(path);
                }
            });

    connect(static_cast<CustomAddAvatarWidget *>(m_avatarFrames[AvatarAdd]),
            &CustomAddAvatarWidget::requestUpdateCustomWidget,
            this,
            [avatarSelectWidget, this](const QString &path) {
                avatarSelectWidget->setCurrentWidget(m_avatarFrames[Custom]);
                m_currentSelectAvatarWidget = m_avatarFrames[Custom];
                m_currentSelectAvatarWidget->getCurrentListView()->requestAddCustomAvatar(path);
            });

    connect(saveButton, &QPushButton::clicked, this, [this]() {
        const QString path = getAvatarPath();
        if (!path.isEmpty() && path != m_curUser->currentAvatar()) {
            Q_EMIT requestSaveAvatar(path);

            // 成功设置头像后关闭窗口
            close();
        }
    });
    connect(cancelButton, &QPushButton::clicked, this, &AvatarListDialog::close);

    m_rightContentLayout->addLayout(btnLayout);

    QFrame *frame = new QFrame(this);
    frame->setLayout(m_rightContentLayout);
    QPalette pa(DDialog().palette());
    pa.setColor(QPalette::Base, pa.color(QPalette::Window));
    frame->setAutoFillBackground(true);
    frame->setPalette(pa);

    m_mainContentLayout->addWidget(frame);

    setLayout(m_mainContentLayout);

    setFixedSize(640, 472);
    installEventFilter(this);
}

AvatarListDialog::~AvatarListDialog()
{
    if (m_avatarSelectItemModel) {
        m_avatarSelectItemModel->clear();
        m_avatarSelectItemModel->deleteLater();
        m_avatarSelectItemModel = nullptr;
    }

    m_avatarFrames.clear();
}

CustomAvatarWidget *AvatarListDialog::getCustomAvatarWidget()
{
    return static_cast<CustomAvatarWidget *>(m_avatarFrames[Custom]);
}

QString AvatarListDialog::getAvatarPath() const
{
    return m_currentSelectAvatarWidget->getAvatarPath();
}

void AvatarListDialog::mousePressEvent(QMouseEvent *e)
{
    m_lastPos = e->globalPos();
    QWidget::mousePressEvent(e);
}

void AvatarListDialog::mouseMoveEvent(QMouseEvent *e)
{
    this->move(this->x() + (e->globalX() - m_lastPos.x()),
               this->y() + (e->globalY() - m_lastPos.y()));
    m_lastPos = e->globalPos();
    QWidget::mouseMoveEvent(e);
}

void AvatarListDialog::mouseReleaseEvent(QMouseEvent *e)
{
    m_lastPos = e->globalPos();
    QWidget::mouseReleaseEvent(e);
}
