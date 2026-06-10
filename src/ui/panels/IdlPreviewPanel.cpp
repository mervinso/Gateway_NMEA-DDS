// src/ui/panels/IdlPreviewPanel.cpp
#include "ui/panels/IdlPreviewPanel.hpp"
#include "ui/GatewayController.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFont>

#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeMember.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/detail/dynamic_language_binding.hpp>

#include "mapper/Mapper.hpp"

namespace nmea::ui {

// Convierte un DynamicType a texto IDL.
static QString typeKindToStr(eprosima::fastdds::dds::TypeKind kind) {
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case TK_FLOAT64: return "double";
        case TK_FLOAT32: return "float";
        case TK_INT32:   return "long";
        case TK_UINT32:  return "unsigned long";
        case TK_INT64:   return "long long";
        case TK_CHAR8:   return "char";
        case TK_STRING8: return "string";
        default: return "octet";
    }
}

static QString dynamicTypeToIdl(
        eprosima::fastdds::dds::DynamicType::_ref_type type) {
    using namespace eprosima::fastdds::dds;
    if (!type) return {};

    QString idl = "struct " + QString::fromStdString(std::string(type->get_name())) + " {\n";
    DynamicTypeMembersById members;
    type->get_all_members(members);
    for (auto& [id, member] : members) {
        MemberDescriptor::_ref_type desc = traits<MemberDescriptor>::make_shared();
        member->get_descriptor(desc);
        const QString typeName = desc->type()
                ? typeKindToStr(desc->type()->get_kind()) : "octet";
        idl += "    " + typeName + " "
             + QString::fromStdString(std::string(desc->name())) + ";\n";
    }
    idl += "};";
    return idl;
}

IdlPreviewPanel::IdlPreviewPanel(GatewayController* ctrl, QWidget* parent)
    : QWidget(parent), ctrl_(ctrl) {
    auto* box    = new QGroupBox("③ IDL", this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(box);

    auto* inner = new QVBoxLayout(box);

    trama_label_ = new QLabel("— selecciona una trama —");
    trama_label_->setObjectName("lbl_warn");
    inner->addWidget(trama_label_);

    idl_view_ = new QPlainTextEdit;
    idl_view_->setReadOnly(true);
    idl_view_->setFont(QFont("Monospace", 10));
    idl_view_->setMinimumHeight(120);
    inner->addWidget(idl_view_);

    auto* btn_row = new QHBoxLayout;
    save_btn_    = new QPushButton("📄 Guardar IDL…");
    convert_btn_ = new QPushButton("✅ Convertir");
    convert_btn_->setObjectName("btn_launch");
    convert_btn_->setEnabled(false);
    btn_row->addWidget(save_btn_);
    btn_row->addStretch();
    btn_row->addWidget(convert_btn_);
    inner->addLayout(btn_row);

    connect(save_btn_,    &QPushButton::clicked, this, &IdlPreviewPanel::onSaveClicked);
    connect(convert_btn_, &QPushButton::clicked, this, &IdlPreviewPanel::onConvertClicked);
}

void IdlPreviewPanel::showFormatter(const QString& formatter) {
    formatter_ = formatter;
    trama_label_->setText(formatter.isEmpty() ? "— selecciona una trama —"
                                              : "Trama: " + formatter);
    trama_label_->setObjectName(formatter.isEmpty() ? "lbl_warn" : "lbl_ok");
    idl_view_->setPlainText(buildIdl(formatter));
    convert_btn_->setEnabled(!formatter.isEmpty());
}

QString IdlPreviewPanel::buildIdl(const QString& formatter) const {
    if (formatter.isEmpty()) return {};
    nmea::Mapper mapper(ctrl_->registry());
    auto type = mapper.type_for(formatter.toStdString());
    return dynamicTypeToIdl(type);
}

void IdlPreviewPanel::onSaveClicked() {
    const QString text = idl_view_->toPlainText();
    if (formatter_.isEmpty() || text.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(
            this, "Guardar IDL", formatter_ + ".idl", "IDL Files (*.idl)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&f) << text;
        emit saveIdlRequested(formatter_, text);
    }
}

void IdlPreviewPanel::onConvertClicked() {
    if (!formatter_.isEmpty()) emit convertRequested();
}

}  // namespace nmea::ui
