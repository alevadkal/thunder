#ifndef COORDINATES_H
#define COORDINATES_H

#include "function.h"

#define UV   "UV"

class ProjectionCoord : public ShaderNode {
    Q_OBJECT
    Q_CLASSINFO("Group", "Coordinates")

public:
    Q_INVOKABLE ProjectionCoord() {
        m_ports.push_back(NodePort(this, true, QMetaType::QVector3D, 0, "Output", m_portColors[QMetaType::QVector3D]));
    }

    Vector2 defaultSize() const override {
        return Vector2(150.0f, 30.0f);
    }

    int32_t build(QString &code, QStack<QString> &stack,const AbstractNodeGraph::Link &link, int32_t &depth, int32_t &type) override {
        if(m_position == -1) {
            code += QString("\tvec3 local%1 = (0.5 *( _vertex.xyz / _vertex.w ) + 0.5);\n").arg(depth);
        }
        return ShaderNode::build(code, stack, link, depth, type);
    }
};

class TexCoord : public ShaderNode {
    Q_OBJECT
    Q_CLASSINFO("Group", "Coordinates")

    Q_PROPERTY(int Index READ index WRITE setIndex NOTIFY updated DESIGNABLE true USER true)

public:
    Q_INVOKABLE TexCoord() :
            m_index(0) {

        m_ports.push_back(NodePort(this, true, QMetaType::QVector2D, 0, "Output", m_portColors[QMetaType::QVector2D]));
    }

    Vector2 defaultSize() const override {
        return Vector2(150.0f, 30.0f);
    }

    int32_t build(QString &code, QStack<QString> &stack,const AbstractNodeGraph::Link &link, int32_t &depth, int32_t &type) override {
        stack.push(QString("_uv%1").arg(m_index));

        return ShaderNode::build(code, stack, link, depth, type);
    }

    uint32_t index() const { return m_index; }
    void setIndex(uint32_t index) { m_index = index; emit updated(); }

protected:
    uint32_t m_index;

};

class CoordPanner : public ShaderNode {
    Q_OBJECT
    Q_CLASSINFO("Group", "Coordinates")

    Q_PROPERTY(float X READ valueX WRITE setValueX NOTIFY updated DESIGNABLE true USER true)
    Q_PROPERTY(float Y READ valueY WRITE setValueY NOTIFY updated DESIGNABLE true USER true)

public:
    Q_INVOKABLE CoordPanner() {
        m_speed = Vector2();

        m_ports.push_back(NodePort(this, false, QMetaType::QVector2D, 1, UV, m_portColors[QMetaType::QVector2D]));
        m_ports.push_back(NodePort(this, true,  QMetaType::QVector2D, 0, "Output", m_portColors[QMetaType::QVector2D]));
    }

    Vector2 defaultSize() const override {
        return Vector2(150.0f, 30.0f);
    }

    int32_t build(QString &code, QStack<QString> &stack, const AbstractNodeGraph::Link &link, int32_t &depth, int32_t &type) override {
        if(m_position == -1) {
            const AbstractNodeGraph::Link *l = m_graph->findLink(this, port(1)); // UV
            if(l) {
                ShaderNode *node = static_cast<ShaderNode *>(l->sender);
                if(node) {
                    int32_t l_type = 0;
                    int32_t index = node->build(code, stack, *l, depth, l_type);
                    if(index >= 0) {
                        QString value;
                        if(!stack.isEmpty()) {
                            value = convert(stack.pop(), l_type, type);
                        } else {
                            value = convert("local" + QString::number(index), l_type, type);
                        }
                        value.append(QString(" + vec2(%1, %2) * g.time").arg(QString::number(m_speed.x), QString::number(m_speed.y)));

                        code.append(QString("\tvec2 local%1 = %2;\n").arg(QString::number(depth), value));
                    }
                }
            } else {
                m_graph->reportMessage(this, QString("Missing argument ") + UV);
                return m_position;
            }
        }
        return ShaderNode::build(code, stack, link, depth, type);
    }

    float valueX() const { return m_speed.x; }
    float valueY() const { return m_speed.y; }

    void setValueX(const float value) { m_speed.x = value; emit updated(); }
    void setValueY(const float value) { m_speed.y = value; emit updated(); }

protected:
    Vector2 m_speed;

};

#endif // COORDINATES_H
