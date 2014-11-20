/*
 Copyright (C) 2010-2014 Kristian Duske
 
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

#include "PointHandleRenderer.h"

#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderContext.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Vbo.h"
#include "Renderer/VertexSpec.h"

#include <algorithm>

namespace TrenchBroom {
    namespace Renderer {
        PointHandleRenderer::PointHandleRenderer(const float radius, const size_t iterations) :
        m_sphere(radius, iterations),
        m_test(radius, 16, true),
        m_circle(2.0f * radius, 16, false),
        m_color(1.0f, 1.0f, 1.0f, 1.0f),
        m_occludedColor(1.0f, 1.0f, 1.0f, 0.5f),
        m_highlightColor(1.0f, 1.0f, 1.0f, 1.0f),
        m_renderOccluded(true) {}
        
        void PointHandleRenderer::addPoint(const Vec3f& position) {
            m_points.push_back(position);
        }
        
        void PointHandleRenderer::setPoint(const Vec3f& position) {
            m_points = Vec3f::List(1, position);
        }
        
        void PointHandleRenderer::setPoints(Vec3f::List& positions) {
            using std::swap;
            swap(m_points, positions);
        }

        void PointHandleRenderer::setPoints(const Vec3f::List& positions) {
            m_points = positions;
        }

        void PointHandleRenderer::addHighlight(const Vec3f& position) {
            m_highlights.push_back(position);
        }
        
        void PointHandleRenderer::setHighlight(const Vec3f& position) {
            m_highlights = Vec3f::List(1, position);
        }
        
        void PointHandleRenderer::setHighlights(Vec3f::List& positions) {
            using std::swap;
            swap(m_highlights, positions);
        }
        
        void PointHandleRenderer::setHighlights(const Vec3f::List& positions) {
            m_highlights = positions;
        }

        void PointHandleRenderer::clear() {
            m_points.clear();
            m_highlights.clear();
        }

        void PointHandleRenderer::setRadius(float radius, size_t iterations) {
            m_sphere = Sphere(radius, iterations);
            m_test = Circle(radius, 16, true);
            m_circle = Circle(2.0f * radius, 16, false);
        }
        
        void PointHandleRenderer::setColor(const Color& color) {
            m_color = color;
        }
        
        void PointHandleRenderer::setOccludedColor(const Color& occludedColor) {
            m_occludedColor = occludedColor;
        }
        
        void PointHandleRenderer::setHighlightColor(const Color& highlightColor) {
            m_highlightColor = highlightColor;
        }

        void PointHandleRenderer::setRenderOccluded(const bool renderOccluded) {
            m_renderOccluded = renderOccluded;
        }

        void PointHandleRenderer::doPrepare(Vbo& vbo) {
            m_sphere.prepare(vbo);
            m_test.prepare(vbo);
            m_circle.prepare(vbo);
        }
        
        void PointHandleRenderer::doRender(RenderContext& renderContext) {
            const Camera& camera = renderContext.camera();
            const Camera::Viewport& viewport = camera.unzoomedViewport();
            
            const Mat4x4f projection = orthoMatrix(-1.0f, 1.0f,
                                                   static_cast<float>(viewport.x),
                                                   static_cast<float>(viewport.height),
                                                   static_cast<float>(viewport.width),
                                                   static_cast<float>(viewport.y));
            const Mat4x4f view = viewMatrix(Vec3f::NegZ, Vec3f::PosY);
            ReplaceTransformation ortho(renderContext.transformation(), projection, view);

            ActiveShader shader(renderContext.shaderManager(), Shaders::HandleShader);
            
            Vec3f::List::const_iterator it, end;
            for (it = m_points.begin(), end = m_points.end(); it != end; ++it) {
                const Vec3f& position = *it;
                const Vec3f offset = camera.project(position);
                MultiplyModelMatrix translate(renderContext.transformation(), translationMatrix(offset));
                
                if (m_renderOccluded) {
                    glDisable(GL_DEPTH_TEST);
                    shader.set("Color", m_occludedColor);
                    m_test.render();
                    glEnable(GL_DEPTH_TEST);
                }
                
                shader.set("Color", m_color);
                m_test.render();
            }
            
            glDisable(GL_DEPTH_TEST);
            shader.set("Color", m_highlightColor);
            
            for (it = m_highlights.begin(), end = m_highlights.end(); it != end; ++it) {
                const Vec3f& position = *it;
                const Vec3f offset(camera.project(position), 0.0f);
                MultiplyModelMatrix translate(renderContext.transformation(), translationMatrix(offset));
                m_circle.render();
            }
            glEnable(GL_DEPTH_TEST);
        }
    }
}
