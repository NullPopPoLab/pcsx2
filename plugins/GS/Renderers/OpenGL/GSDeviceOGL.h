/*
 *	Copyright (C) 2011-2013 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include <vector>

#include "Pcsx2Types.h"

#include "../Common/GSDevice.h"
#include "GSTextureOGL.h"
#include "../../GS.h"
#include "GSVertexArrayOGL.h"
#include "GSUniformBufferOGL.h"
#include "GSShaderOGL.h"
#include "GLState.h"

class GSDepthStencilOGL {
	bool m_depth_enable;
	GLenum m_depth_func;
	bool m_depth_mask;
	// Note front face and back might be split but it seems they have same parameter configuration
	bool m_stencil_enable;
	GLenum m_stencil_func;
	GLenum m_stencil_spass_dpass_op;

public:

	GSDepthStencilOGL() : m_depth_enable(false)
		, m_depth_func(GL_ALWAYS)
		, m_depth_mask(0)
		, m_stencil_enable(false)
		, m_stencil_func(0)
		, m_stencil_spass_dpass_op(GL_KEEP)
	{
	}

	void EnableDepth() { m_depth_enable = true; }
	void EnableStencil() { m_stencil_enable = true; }

	void SetDepth(GLenum func, bool mask) { m_depth_func = func; m_depth_mask = mask; }
	void SetStencil(GLenum func, GLenum pass) { m_stencil_func = func; m_stencil_spass_dpass_op = pass; }

	void SetupDepth()
	{
		if (GLState::depth != m_depth_enable) {
			GLState::depth = m_depth_enable;
			if (m_depth_enable)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}

		if (m_depth_enable) {
			if (GLState::depth_func != m_depth_func) {
				GLState::depth_func = m_depth_func;
				glDepthFunc(m_depth_func);
			}
			if (GLState::depth_mask != m_depth_mask) {
				GLState::depth_mask = m_depth_mask;
				glDepthMask((GLboolean)m_depth_mask);
			}
		}
	}

	void SetupStencil()
	{
		if (GLState::stencil != m_stencil_enable) {
			GLState::stencil = m_stencil_enable;
			if (m_stencil_enable)
				glEnable(GL_STENCIL_TEST);
			else
				glDisable(GL_STENCIL_TEST);
		}

		if (m_stencil_enable) {
			// Note: here the mask control which bitplane is considered by the operation
			if (GLState::stencil_func != m_stencil_func) {
				GLState::stencil_func = m_stencil_func;
				glStencilFunc(m_stencil_func, 1, 1);
			}
			if (GLState::stencil_pass != m_stencil_spass_dpass_op) {
				GLState::stencil_pass = m_stencil_spass_dpass_op;
				glStencilOp(GL_KEEP, GL_KEEP, m_stencil_spass_dpass_op);
			}
		}
	}

	bool IsMaskEnable() { return m_depth_mask != GL_FALSE; }
};

class GSDeviceOGL final : public GSDevice
{
public:
	struct alignas(32) VSConstantBuffer
	{
		GSVector4 Vertex_Scale_Offset;

		GSVector4 TextureOffset;

		GSVector2 PointSize;
		GSVector2i MaxDepth;

		VSConstantBuffer()
		{
			Vertex_Scale_Offset = GSVector4::zero();
			TextureOffset       = GSVector4::zero();
			PointSize           = GSVector2(0);
			MaxDepth            = GSVector2i(0);
		}

		__forceinline bool Update(const VSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			if(!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2])).alltrue())
			{
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];

				return true;
			}

			return false;
		}
	};

	struct VSSelector
	{
		union
		{
			struct
			{
				u32 int_fst:1;
				u32 _free:31;
			};

			u32 key;
		};

		operator u32() const {return key;}

		VSSelector() : key(0) {}
		VSSelector(u32 k) : key(k) {}
	};

	struct GSSelector
	{
		union
		{
			struct
			{
				u32 sprite:1;
				u32 point:1;
				u32 line:1;

				u32 _free:29;
			};

			u32 key;
		};

		operator u32() const {return key;}

		GSSelector() : key(0) {}
		GSSelector(u32 k) : key(k) {}
	};

	struct alignas(32) PSConstantBuffer
	{
		GSVector4 FogColor_AREF;
		GSVector4 WH;
		GSVector4 TA_Af;
		GSVector4i MskFix;
		GSVector4i FbMask;

		GSVector4 HalfTexel;
		GSVector4 MinMax;
		GSVector4 TC_OH_TS;
		GSVector4 MaxDepth;

		GSVector4 DitherMatrix[4];

		PSConstantBuffer()
		{
			FogColor_AREF = GSVector4::zero();
			HalfTexel     = GSVector4::zero();
			WH            = GSVector4::zero();
			TA_Af         = GSVector4::zero();
			MinMax        = GSVector4::zero();
			MskFix        = GSVector4i::zero();
			TC_OH_TS      = GSVector4::zero();
			FbMask        = GSVector4i::zero();
			MaxDepth      = GSVector4::zero();

			DitherMatrix[0] = GSVector4::zero();
			DitherMatrix[1] = GSVector4::zero();
			DitherMatrix[2] = GSVector4::zero();
			DitherMatrix[3] = GSVector4::zero();
		}

		__forceinline bool Update(const PSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			// if WH matches both HalfTexel and TC_OH_TS do too
         if (!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2]) & (a[3] == b[3]) & (a[4] == b[4]) & (a[6] == b[6])
				& (a[8] == b[8]) & (a[9] == b[9]) & (a[10] == b[10]) & (a[11] == b[11]) & (a[12] == b[12])).alltrue())
			{
				// Note previous check uses SSE already, a plain copy will be faster than any memcpy
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];
				a[3] = b[3];
				a[4] = b[4];
				a[5] = b[5];
            a[6] = b[6];

				a[8] = b[8];

				a[9] = b[9];
				a[10] = b[10];
				a[11] = b[11];
				a[12] = b[12];

				return true;
			}

			return false;
		}
	};

	struct PSSelector
	{
		// Performance note: there are too many shader combinations
		// It might hurt the performance due to frequent toggling worse it could consume
		// a lots of memory.
		union
		{
			struct
			{
				// *** Word 1
				// Format
				u32 tex_fmt:4;
				u32 dfmt:2;
				u32 depth_fmt:2;
				// Alpha extension/Correction
				u32 aem:1;
				u32 fba:1;
				// Fog
				u32 fog:1;
				// Flat/goround shading
				u32 iip:1;
				// Pixel test
				u32 date:3;
				u32 atst:3;
				// Color sampling
				u32 fst:1; // Investigate to do it on the VS
				u32 tfx:3;
				u32 tcc:1;
				u32 wms:2;
				u32 wmt:2;
				u32 ltf:1;
				// Shuffle and fbmask effect
				u32 shuffle:1;
				u32 read_ba:1;
				u32 write_rg:1;
				u32 fbmask:1;

				//u32 _free1:0;

				// *** Word 2
				// Blend and Colclip
				u32 blend_a:2;
				u32 blend_b:2;
				u32 blend_c:2;
				u32 blend_d:2;
				u32 clr1:1; // useful?
				u32 hdr:1;
				u32 colclip:1;
				u32 pabe:1;

				// Others ways to fetch the texture
				u32 channel:3;

				// Dithering
				u32 dither:2;

				// Depth clamp
				u32 zclamp:1;

				// Hack
				u32 tcoffsethack:1;
				u32 urban_chaos_hle:1;
				u32 tales_of_abyss_hle:1;
				u32 tex_is_fb:1; // Jak Shadows
				u32 automatic_lod:1;
				u32 manual_lod:1;
				u32 point_sampler:1;
				u32 invalid_tex0:1; // Lupin the 3rd

				u32 _free2:6;
			};

			u64 key;
		};

		// FIXME is the & useful ?
		operator u64() const {return key;}

		PSSelector() : key(0) {}
	};

	struct PSSamplerSelector
	{
		union
		{
			struct
			{
				u32 tau:1;
				u32 tav:1;
				u32 biln:1;
				u32 triln:3;
				u32 aniso:1;

				u32 _free:25;
			};

			u32 key;
		};

		operator u32() {return key;}

		PSSamplerSelector() : key(0) {}
		PSSamplerSelector(u32 k) : key(k) {}
	};

	struct OMDepthStencilSelector
	{
		union
		{
			struct
			{
				u32 ztst:2;
				u32 zwe:1;
				u32 date:1;
				u32 date_one:1;

				u32 _free:27;
			};

			u32 key;
		};

		// FIXME is the & useful ?
		operator u32() {return key;}

		OMDepthStencilSelector() : key(0) {}
		OMDepthStencilSelector(u32 k) : key(k) {}
	};

	struct OMColorMaskSelector
	{
		union
		{
			struct
			{
				u32 wr:1;
				u32 wg:1;
				u32 wb:1;
				u32 wa:1;

				u32 _free:28;
			};

			struct
			{
				u32 wrgba:4;
			};

			u32 key;
		};

		// FIXME is the & useful ?
		operator u32() {return key & 0xf;}

		OMColorMaskSelector() : key(0xF) {}
		OMColorMaskSelector(u32 c) { wrgba = c; }
	};

	struct alignas(32) MiscConstantBuffer
	{
		GSVector4i ScalingFactor;
		GSVector4i ChannelShuffle;
		GSVector4i EMOD_AC;

		MiscConstantBuffer() {memset(this, 0, sizeof(*this));}
	};

	static int m_shader_inst;
	static int m_shader_reg;

private:
	int m_force_texture_clear;
	int m_mipmap;
	TriFiltering m_filter;

	bool m_disable_hw_gl_draw;

	// Place holder for the GLSL shader code (to avoid useless reload)
	std::vector<char> m_shader_tfx_vgs;
	std::vector<char> m_shader_tfx_fs;

	GLuint m_fbo;				// frame buffer container
	GLuint m_fbo_read;			// frame buffer container only for reading

	GSVertexBufferStateOGL* m_va;// state of the vertex buffer/array

	struct {
		GLuint ps[2];				 // program object
		GSUniformBufferOGL* cb;		 // uniform buffer object
	} m_merge_obj;

	struct {
		GLuint ps[4];				// program object
		GSUniformBufferOGL* cb;		// uniform buffer object
	} m_interlace;

	struct {
		GLuint vs;		// program object
		GLuint ps[ShaderConvert_Count];	// program object
		GLuint ln;		// sampler object
		GLuint pt;		// sampler object
		GSDepthStencilOGL* dss;
		GSDepthStencilOGL* dss_write;
		GSUniformBufferOGL* cb;
	} m_convert;

	struct {
		GLuint ps;
		GSUniformBufferOGL *cb;
	} m_fxaa;

	struct {
		GSDepthStencilOGL* dss;
		GSTexture* t;
	} m_date;

	struct {
		u16 last_query;
		GLuint timer_query[1<<16];

		GLuint timer() { return timer_query[last_query]; }
	} m_profiler;

	GLuint m_vs[1<<1];
	GLuint m_gs[1<<3];
	GLuint m_ps_ss[1<<7];
	GSDepthStencilOGL* m_om_dss[1<<5];
	std::unordered_map<u64, GLuint> m_ps;
	GLuint m_apitrace;

	GLuint m_palette_ss;

	GSUniformBufferOGL* m_vs_cb;
	GSUniformBufferOGL* m_ps_cb;

	VSConstantBuffer m_vs_cb_cache;
	PSConstantBuffer m_ps_cb_cache;
	MiscConstantBuffer m_misc_cb_cache;
	GSTexture* CreateSurface(int type, int w, int h, int format);
	GSTexture* FetchSurface(int type, int w, int h, int format);

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) final;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;

	void OMAttachRt(GSTextureOGL* rt = NULL);
	void OMAttachDs(GSTextureOGL* ds = NULL);
	void OMSetFBO(GLuint fbo);

	u16 ConvertBlendEnum(u16 generic) final;

public:
	GSShaderOGL* m_shader;

	GSDeviceOGL();
	virtual ~GSDeviceOGL();

	bool Create();
	bool Reset(int w, int h);
	void Flip();

	void DrawPrimitive();
	void DrawPrimitive(int offset, int count);
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) final;
	void ClearRenderTarget(GSTexture* t, u32 c) final;
	void ClearDepth(GSTexture* t) final;
	void ClearStencil(GSTexture* t, u8 c) final;

	void InitPrimDateTexture(GSTexture* rt, const GSVector4i& area);
	void RecycleDateTexture();

	GSTexture* CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format = 0, int ps_shader = 0) final;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r) final;
	void CopyRectConv(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, bool at_origin);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader = 0, bool linear = true) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, int bs, OMColorMaskSelector cms, bool linear = true);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void BeginScene() final {}
	void EndScene() final;

	void IASetPrimitiveTopology(GLenum topology);
	void IASetVertexBuffer(const void* vertices, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1);
	void PSSetSamplerState(GLuint ss);

	void OMSetDepthStencilState(GSDepthStencilOGL* dss);
	void OMSetBlendState(u8 blend_index = 0, u8 blend_factor = 0, bool is_blend_constant = false, bool accumulation_blend = false);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL);
	void OMSetColorMaskState(OMColorMaskSelector sel = OMColorMaskSelector());

	virtual bool HasColorSparse() { return GLLoader::found_compatible_GL_ARB_sparse_texture2; }
	virtual bool HasDepthSparse() { return GLLoader::found_compatible_sparse_depth; }

	void CreateTextureFX();
	GLuint CompileVS(VSSelector sel);
	GLuint CompileGS(GSSelector sel);
	GLuint CompilePS(PSSelector sel);
	GLuint CreateSampler(PSSamplerSelector sel);
	GSDepthStencilOGL* CreateDepthStencil(OMDepthStencilSelector dssel);

	void SetupPipeline(const VSSelector& vsel, const GSSelector& gsel, const PSSelector& psel);
	void SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb);
	void SetupCBMisc(const GSVector4i& channel);
	void SetupSampler(PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel);
	GLuint GetSamplerID(PSSamplerSelector ssel);
	GLuint GetPaletteSamplerID();

	void Barrier(GLbitfield b);
};
