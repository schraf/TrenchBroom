/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "NodeSerializer.h"

#include "Model/BrushFace.h"
#include "Model/BrushNode.h"
#include "Model/GroupNode.h"
#include "Model/EntityProperties.h"
#include "Model/LayerNode.h"
#include "Model/LockState.h"
#include "Model/WorldNode.h"

#include <vecmath/vec_io.h> // for Color stream output operator

#include <kdl/overload.h>
#include <kdl/string_format.h>
#include <kdl/string_utils.h>

#include <string>

namespace TrenchBroom {
    namespace IO {
        const std::string& NodeSerializer::IdManager::getId(const Model::Node* t) const {
            auto it = m_ids.find(t);
            if (it == std::end(m_ids)) {
                it = m_ids.insert(std::make_pair(t, idToString(makeId()))).first;
            }
            return it->second;
        }

        Model::IdType NodeSerializer::IdManager::makeId() const {
            static Model::IdType currentId = 1;
            return currentId++;
        }

        std::string NodeSerializer::IdManager::idToString(const Model::IdType nodeId) const {
            return kdl::str_to_string(nodeId);
        }

        NodeSerializer::NodeSerializer() :
        m_entityNo(0),
        m_brushNo(0),
        m_exporting(false) {}

        NodeSerializer::~NodeSerializer() = default;

        NodeSerializer::ObjectNo NodeSerializer::entityNo() const {
            return m_entityNo;
        }

        NodeSerializer::ObjectNo NodeSerializer::brushNo() const {
            return m_brushNo;
        }

        bool NodeSerializer::exporting() const {
            return m_exporting;
        }

        void NodeSerializer::setExporting(const bool exporting) {
            m_exporting = exporting;
        }

        void NodeSerializer::beginFile(const std::vector<const Model::Node*>& rootNodes) {
            m_entityNo = 0;
            m_brushNo = 0;
            doBeginFile(rootNodes);
        }

        void NodeSerializer::endFile() {
            doEndFile();
        }

        /**
         * Writes the worldspawn entity.
         */
        void NodeSerializer::defaultLayer(const Model::WorldNode& world) {
            auto worldEntity = world.entity();

            // Transfer the color, locked state, and hidden state from the default layer Layer object to worldspawn
            const Model::LayerNode* defaultLayerNode = world.defaultLayer();
            const Model::Layer& defaultLayer = defaultLayerNode->layer();
            if (defaultLayer.color()) {
                worldEntity.addOrUpdateProperty(Model::PropertyKeys::LayerColor,
                    kdl::str_to_string(*defaultLayer.color()));
            } else {
                worldEntity.removeProperty(Model::PropertyKeys::LayerColor);
            }

            if (defaultLayerNode->lockState() == Model::LockState::Lock_Locked) {
                worldEntity.addOrUpdateProperty(Model::PropertyKeys::LayerLocked,
                    Model::PropertyValues::LayerLockedValue);
            } else {
                worldEntity.removeProperty(Model::PropertyKeys::LayerLocked);
            }

            if (defaultLayerNode->hidden()) {
                worldEntity.addOrUpdateProperty(Model::PropertyKeys::LayerHidden,
                    Model::PropertyValues::LayerHiddenValue);
            } else {
                worldEntity.removeProperty(Model::PropertyKeys::LayerHidden);
            }

            if (defaultLayer.omitFromExport()) {
                worldEntity.addOrUpdateProperty(Model::PropertyKeys::LayerOmitFromExport,
                    Model::PropertyValues::LayerOmitFromExportValue);
            } else {
                worldEntity.removeProperty(Model::PropertyKeys::LayerOmitFromExport);
            }

            if (m_exporting && defaultLayer.omitFromExport()) {
                beginEntity(&world, worldEntity.properties(), {});
                endEntity(&world);
            } else {
                entity(&world, worldEntity.properties(), {}, world.defaultLayer());
            }
        }

        void NodeSerializer::customLayer(const Model::LayerNode* layer) {
            if (!(m_exporting && layer->layer().omitFromExport())) {
                entity(layer, layerProperties(layer), {}, layer);
            }
        }

        void NodeSerializer::group(const Model::GroupNode* group, const std::vector<Model::EntityProperty>& parentProperties) {
            entity(group, groupProperties(group), parentProperties, group);
        }

        void NodeSerializer::entity(const Model::Node* node, const std::vector<Model::EntityProperty>& properties, const std::vector<Model::EntityProperty>& parentProperties, const Model::Node* brushParent) {
            beginEntity(node, properties, parentProperties);

            brushParent->visitChildren(kdl::overload(
                [] (const Model::WorldNode*)   {},
                [] (const Model::LayerNode*)   {},
                [] (const Model::GroupNode*)   {},
                [] (const Model::EntityNode*)  {},
                [&](const Model::BrushNode* b) {
                    brush(b);
                }
            ));

            endEntity(node);
        }

        void NodeSerializer::entity(const Model::Node* node, const std::vector<Model::EntityProperty>& properties, const std::vector<Model::EntityProperty>& parentProperties, const std::vector<Model::BrushNode*>& entityBrushes) {
            beginEntity(node, properties, parentProperties);
            brushes(entityBrushes);
            endEntity(node);
        }

        void NodeSerializer::beginEntity(const Model::Node* node, const std::vector<Model::EntityProperty>& properties, const std::vector<Model::EntityProperty>& extraAttributes) {
            beginEntity(node);
            entityProperties(properties);
            entityProperties(extraAttributes);
        }

        void NodeSerializer::beginEntity(const Model::Node* node) {
            m_brushNo = 0;
            doBeginEntity(node);
        }

        void NodeSerializer::endEntity(const Model::Node* node) {
            doEndEntity(node);
            ++m_entityNo;
        }

        void NodeSerializer::entityProperties(const std::vector<Model::EntityProperty>& properties) {
            for (const auto& property : properties) {
                entityProperty(property);
            }
        }

        void NodeSerializer::entityProperty(const Model::EntityProperty& property) {
            doEntityProperty(property);
        }

        void NodeSerializer::brushes(const std::vector<Model::BrushNode*>& brushNodes) {
            for (auto* brush : brushNodes) {
                this->brush(brush);
            }
        }

        void NodeSerializer::brush(const Model::BrushNode* brushNode) {
            doBrush(brushNode);
            ++m_brushNo;
        }

        void NodeSerializer::brushFaces(const std::vector<Model::BrushFace>& faces) {
            for (const auto& face : faces) {
                brushFace(face);
            }
        }

        void NodeSerializer::brushFace(const Model::BrushFace& face) {
            doBrushFace(face);
        }

        std::vector<Model::EntityProperty> NodeSerializer::parentProperties(const Model::Node* node) {
            if (node == nullptr) {
                return std::vector<Model::EntityProperty>{};
            }

            auto properties = std::vector<Model::EntityProperty>{};
            node->accept(kdl::overload(
                [](const Model::WorldNode*) {},
                [&](const Model::LayerNode* layer) { properties.push_back(Model::EntityProperty(Model::PropertyKeys::Layer, m_layerIds.getId(layer))); },
                [&](const Model::GroupNode* group) { properties.push_back(Model::EntityProperty(Model::PropertyKeys::Group, m_groupIds.getId(group))); },
                [](const Model::EntityNode*) {},
                [](const Model::BrushNode*) {}
            ));

            return properties;
        }

        std::vector<Model::EntityProperty> NodeSerializer::layerProperties(const Model::LayerNode* layerNode) {
            std::vector<Model::EntityProperty> result = {
                Model::EntityProperty(Model::PropertyKeys::Classname, Model::PropertyValues::LayerClassname),
                Model::EntityProperty(Model::PropertyKeys::GroupType, Model::PropertyValues::GroupTypeLayer),
                Model::EntityProperty(Model::PropertyKeys::LayerName, layerNode->name()),
                Model::EntityProperty(Model::PropertyKeys::LayerId, m_layerIds.getId(layerNode)),
            };

            const auto& layer = layerNode->layer();
            if (layer.hasSortIndex()) {
                result.push_back(Model::EntityProperty(Model::PropertyKeys::LayerSortIndex, kdl::str_to_string(layer.sortIndex())));
            }
            if (layerNode->lockState() == Model::LockState::Lock_Locked) {
                result.push_back(Model::EntityProperty(Model::PropertyKeys::LayerLocked, Model::PropertyValues::LayerLockedValue));
            }
            if (layerNode->hidden()) {
                result.push_back(Model::EntityProperty(Model::PropertyKeys::LayerHidden, Model::PropertyValues::LayerHiddenValue));
            }
            if (layer.omitFromExport()) {
                result.push_back(Model::EntityProperty(Model::PropertyKeys::LayerOmitFromExport, Model::PropertyValues::LayerOmitFromExportValue));
            }
            return result;
        }

        std::vector<Model::EntityProperty> NodeSerializer::groupProperties(const Model::GroupNode* group) {
            return {
                Model::EntityProperty(Model::PropertyKeys::Classname, Model::PropertyValues::GroupClassname),
                Model::EntityProperty(Model::PropertyKeys::GroupType, Model::PropertyValues::GroupTypeGroup),
                Model::EntityProperty(Model::PropertyKeys::GroupName, group->name()),
                Model::EntityProperty(Model::PropertyKeys::GroupId, m_groupIds.getId(group)),
            };
        }

        std::string NodeSerializer::escapeEntityProperties(const std::string& str) const {
            // Remove a trailing unescaped backslash, as this will choke the parser.
            const auto l = str.size();
            if (l > 0 && str[l-1] == '\\') {
                const auto p = str.find_last_not_of('\\');
                if ((l - p) % 2 == 0) {
                    // Only remove a trailing backslash if there is an uneven number of trailing backslashes.
                    return kdl::str_escape_if_necessary(str.substr(0, l-1), "\"");
                }
            }
            return kdl::str_escape_if_necessary(str, "\"");
        }
    }
}
