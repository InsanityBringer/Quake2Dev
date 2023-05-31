/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2023 SaladBadger

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "vk_local.h"

image_t		vktextures[MAX_GLTEXTURES];
int numvktextures;

static byte			 intensitytable[256];
static unsigned char gammatable[256];

cvar_t* intensity;

unsigned	d_8to24table[256];

//probably won't allow direct upload of 8 bit images, since I'm not sure Vulkan supports 8-bit indexed textures directly,
//and writing a shader specifically for it is probably not worth it.
bool VK_Upload8(byte* data, image_t* image, int width, int height, bool mipmap, bool is_sky, unsigned int* palette);
bool VK_Upload32(unsigned* data, image_t* image, int width, int height, bool mipmap);

int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_tex_solid_format = 3;
int		gl_tex_alpha_format = 4;

void VK_SetTexturePalette(unsigned palette[256])
{
}

/*
===============
VK_ImageList_f
===============
*/
void	VK_ImageList_f(void)
{
	int		i;
	image_t* image;
	int		texels;
	const char* palstrings[2] =
	{
		"RGB",
		"PAL"
	};

	int currentpage = 0;

	ri.Con_Printf(PRINT_ALL, "------------------\n");
	texels = 0;

	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (image->handle == VK_NULL_HANDLE)
			continue;
		texels += image->upload_width * image->upload_height;
		switch (image->type)
		{
		case it_skin:
			ri.Con_Printf(PRINT_ALL, "M");
			break;
		case it_sprite:
			ri.Con_Printf(PRINT_ALL, "S");
			break;
		case it_wall:
			ri.Con_Printf(PRINT_ALL, "W");
			break;
		case it_pic:
			ri.Con_Printf(PRINT_ALL, "P");
			break;
		default:
			ri.Con_Printf(PRINT_ALL, " ");
			break;
		}

		ri.Con_Printf(PRINT_ALL, " %3i %3i %s: %s\n",
			image->upload_width, image->upload_height, palstrings[image->paletted], image->name);
	}
	ri.Con_Printf(PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}

enum class vk_texturemode_e
{
	GL_NEAREST,
	GL_LINEAR,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_LINEAR
};

struct vk_texturemode_t
{
	const char* str;
	vk_texturemode_e mode;
} texture_modes[6] =
{
	{"GL_NEAREST", vk_texturemode_e::GL_NEAREST},
	{"GL_LINEAR", vk_texturemode_e::GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", vk_texturemode_e::GL_NEAREST_MIPMAP_NEAREST},
	{"GL_NEAREST_MIPMAP_LINEAR", vk_texturemode_e::GL_NEAREST_MIPMAP_LINEAR},
	{"GL_LINEAR_MIPMAP_NEAREST", vk_texturemode_e::GL_LINEAR_MIPMAP_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", vk_texturemode_e::GL_LINEAR_MIPMAP_LINEAR}
};

vk_texturemode_e current_texture_mode;

void VK_SetTextureMode()
{
	for (int i = 0; i < 6; i++)
	{
		if (!stricmp(vk_texturemode->string, texture_modes[i].str))
		{
			current_texture_mode = texture_modes[i].mode;
			return;
		}
	}

	current_texture_mode = vk_texturemode_e::GL_NEAREST;
}

VkSampler GetSamplerForImage(image_t* image)
{
	switch (current_texture_mode)
	{
	case vk_texturemode_e::GL_NEAREST:
		return vk_state.device.nearest_sampler;
	case vk_texturemode_e::GL_LINEAR:
		return vk_state.device.linear_sampler;
	case vk_texturemode_e::GL_NEAREST_MIPMAP_NEAREST:
		if (image->miplevels == 1) //TODO: is this really needed
			return vk_state.device.nearest_sampler;
		return vk_state.device.nearest_mipmap_nearest_sampler;
	case vk_texturemode_e::GL_NEAREST_MIPMAP_LINEAR:
		if (image->miplevels == 1)
			return vk_state.device.nearest_sampler;
		return vk_state.device.nearest_mipmap_linear_sampler;
	case vk_texturemode_e::GL_LINEAR_MIPMAP_NEAREST:
		if (image->miplevels == 1)
			return vk_state.device.linear_sampler;
		return vk_state.device.linear_mipmap_nearest_sampler;
	case vk_texturemode_e::GL_LINEAR_MIPMAP_LINEAR:
		if (image->miplevels == 1)
			return vk_state.device.linear_sampler;
		return vk_state.device.linear_mipmap_linear_sampler;
	}
}

/*
================
VK_AssignImageName

Gives the passed image a debug name. I need to unify creation of images..
================
*/
void VK_AssignImageName(VkImage image, const char* name)
{
	if (!debug_utils_available)
		return;
	VkDebugUtilsObjectNameInfoEXT nameinfo = {};
	nameinfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameinfo.objectType = VK_OBJECT_TYPE_IMAGE;
	nameinfo.pObjectName = name;
	nameinfo.objectHandle = image;

	VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk_state.device.handle, &nameinfo));
}

/*
================
VK_AssignViewName

Gives the passed image view a debug name. I need to unify creation of images..
================
*/
void VK_AssignViewName(VkImageView image, const char* name)
{
	if (!debug_utils_available)
		return;
	VkDebugUtilsObjectNameInfoEXT nameinfo = {};
	nameinfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameinfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
	nameinfo.pObjectName = name;
	nameinfo.objectHandle = image;

	VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk_state.device.handle, &nameinfo));
}

/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
void LoadPCX(const char* filename, byte** pic, byte** palette, int* width, int* height)
{
	byte* raw;
	pcx_t* pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte* out, * pix;

	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	len = ri.FS_LoadFile(filename, (void**)&raw);
	if (!raw)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t*)raw;

	pcx->xmin = LittleShort(pcx->xmin);
	pcx->ymin = LittleShort(pcx->ymin);
	pcx->xmax = LittleShort(pcx->xmax);
	pcx->ymax = LittleShort(pcx->ymax);
	pcx->hres = LittleShort(pcx->hres);
	pcx->vres = LittleShort(pcx->vres);
	pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		ri.Con_Printf(PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	out = (byte*)malloc((pcx->ymax + 1) * (pcx->xmax + 1));
	if (!out)
	{
		ri.Sys_Error(ERR_FATAL, "Failed to allocate memory for PCX image");
		return;
	}

	*pic = out;

	pix = out;

	if (palette)
	{
		//[ISB] Since I'm actually using the palette from the image on uploads, expand it to 32 bit for easier upload
		*palette = (byte*)malloc(1024);
		
		int src = 0;
		byte* pal = *palette;
		for (int i = 0; i < 1024; i += 4, src += 3)
		{
			pal[i] = *((byte*)pcx + len - 768 + src);
			pal[i + 1] = *((byte*)pcx + len - 768 + src + 1);
			pal[i + 2] = *((byte*)pcx + len - 768 + src + 2);
			pal[i + 3] = 255;
		}
		pal[1023] = 0; //palette index 255 is always transparent. 
	}

	if (width)
		*width = pcx->xmax + 1;
	if (height)
		*height = pcx->ymax + 1;

	for (y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1)
	{
		for (x = 0; x <= pcx->xmax; )
		{
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if (raw - (byte*)pcx > len)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free(*pic);
		*pic = NULL;
	}

	ri.FS_FreeFile(pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader 
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/
void LoadTGA(const char* name, byte** pic, int* width, int* height)
{
	int		columns, rows, numPixels;
	byte* pixbuf;
	int		row, column;
	byte* buf_p;
	byte* buffer;
	int		length;
	TargaHeader		targa_header;
	byte* targa_rgba;
	byte tmp[2];

	*pic = NULL;

	//
	// load the file
	//
	length = ri.FS_LoadFile(name, (void**)&buffer);
	if (!buffer)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "Bad tga file %s\n", name);
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort(*((short*)tmp));
	buf_p += 2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort(*((short*)tmp));
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort(*((short*)buf_p));
	buf_p += 2;
	targa_header.y_origin = LittleShort(*((short*)buf_p));
	buf_p += 2;
	targa_header.width = LittleShort(*((short*)buf_p));
	buf_p += 2;
	targa_header.height = LittleShort(*((short*)buf_p));
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type != 2
		&& targa_header.image_type != 10)
		ri.Sys_Error(ERR_DROP, "LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type != 0
		|| (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		ri.Sys_Error(ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = (byte*)malloc(numPixels * 4);
	if (!targa_rgba)
	{
		ri.Sys_Error(ERR_FATAL, "Failed to allocate memory for TGA image");
		return;
	}
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment

	if (targa_header.image_type == 2) {  // Uncompressed, RGB images
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row * columns * 4;
			for (column = 0; column < columns; column++) {
				unsigned char red, green, blue, alphabyte;
				switch (targa_header.pixel_size) {
				case 24:

					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10) {   // Runlength encoded RGB images
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row * columns * 4;
			for (column = 0; column < columns; ) {
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = 255;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						break;
					}

					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) { // run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else {                            // non run-length packet
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						}
						column++;
						if (column == columns) { // pixel packet run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
		breakOut:;
		}
	}

	ri.FS_FreeFile(buffer);
}

//=======================================================
/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void R_FloodFillSkin(byte* skin, int skinwidth, int skinheight)
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	static floodfill_t	fifo[FLOODFILL_FIFO_SIZE]; //[ISB] static to avoid stack usage. 
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte* pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)	FLOODFILL_STEP(1, 1, 0);
		if (y > 0)				FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)	FLOODFILL_STEP(skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================

/*
================
VK_ResampleTexture

Not really needed for Vulkan, but I may support it for an accurate look.
================
*/
void VK_ResampleTexture(unsigned* in, int inwidth, int inheight, unsigned* out, int outwidth, int outheight)
{
	int		i, j;
	unsigned* inrow, * inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	byte* pix1, * pix2, * pix3, * pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte*)inrow + p1[j];
			pix2 = (byte*)inrow + p2[j];
			pix3 = (byte*)inrow2 + p1[j];
			pix4 = (byte*)inrow2 + p2[j];
			((byte*)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte*)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte*)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte*)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
================
VK_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range

TODO: This should probably be done in a shader rather than at upload time for additional precision. 
================
*/
void VK_LightScaleTexture(unsigned* in, int inwidth, int inheight, bool only_gamma)
{
	if (only_gamma)
	{
		int		i, c;
		byte* p;

		p = (byte*)in;

		c = inwidth * inheight;
		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int		i, c;
		byte* p;

		p = (byte*)in;

		c = inwidth * inheight;
		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[intensitytable[p[0]]];
			p[1] = gammatable[intensitytable[p[1]]];
			p[2] = gammatable[intensitytable[p[2]]];
		}
	}
}

/*
================
VK_CreateVKImageFromImage

Creates the image that will hold the image data, with the specified width and height (may not match image->width and image->height)
ATM called at upload time since this is when the scaled size (if different), mip levels, and so on are counted. May be better done elsewhere.
================
*/
void VK_CreateVKImageFromImage(image_t* image)
{
	VkImageCreateInfo imageinfo = {};
	imageinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageinfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageinfo.extent.width = image->upload_width;
	imageinfo.extent.height = image->upload_height;
	imageinfo.extent.depth = 1;
	imageinfo.arrayLayers = 1;
	imageinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageinfo.mipLevels = image->miplevels;
	imageinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageinfo.imageType = VK_IMAGE_TYPE_2D;
	imageinfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateImage(vk_state.device.handle, &imageinfo, NULL, &image->handle));

	VK_AssignImageName(image->handle, image->name);

	VkMemoryRequirements reqs;
	vkGetImageMemoryRequirements(vk_state.device.handle, image->handle, &reqs);

	image->memory = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	image->data_size = 4;
	vkBindImageMemory(vk_state.device.handle, image->handle, image->memory.memory, image->memory.offset);

	VkImageViewCreateInfo viewinfo = {};
	viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	viewinfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewinfo.image = image->handle;
	viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewinfo.subresourceRange.layerCount = 1;
	viewinfo.subresourceRange.levelCount = image->miplevels;
	viewinfo.subresourceRange.baseArrayLayer = 0;
	viewinfo.subresourceRange.baseMipLevel = 0;
	viewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewinfo.components = {
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY };

	VK_CHECK(vkCreateImageView(vk_state.device.handle, &viewinfo, NULL, &image->viewhandle));

	VK_AssignViewName(image->viewhandle, image->name);

	//Recycle the former descriptor if it exists
	if (image->descriptor.set == VK_NULL_HANDLE)
	{
		//Create the descriptor set now and update it
		VK_AllocateDescriptor(&vk_state.device.image_descriptor_pool, 1, &vk_state.device.sampled_texture_layout, &image->descriptor);
	}

	VkDescriptorImageInfo dimageinfo = {};
	dimageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dimageinfo.imageView = image->viewhandle;
	dimageinfo.sampler = GetSamplerForImage(image);

	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = image->descriptor.set;
	writeinfo.pImageInfo = &dimageinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);
}

/*
================
VK_FreeImage

Frees the image, the image view, its associated memory, and the descriptor set used with it.
================
*/
void VK_FreeImage(image_t* image)
{
	//undone: can't free descriptors without free bit. Eh. 
	//VK_FreeDescriptors(1, &image->descriptor);

	VK_FreeMemory(&vk_state.device.device_local_pool, &image->memory);

	vkDestroyImageView(vk_state.device.handle, image->viewhandle, NULL);
	vkDestroyImage(vk_state.device.handle, image->handle, NULL);

	//Recycle the descriptor
	vkdescriptorset_t backup = image->descriptor;
	memset(image, 0, sizeof(*image));
	image->descriptor = backup;
}

/*
================
VK_MipMap

Operates in place, quartering the size of the texture
================
*/
void VK_MipMap(byte* in, int width, int height)
{
	int		i, j;
	byte* out;

	width <<= 2;
	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

//[ISB] knock the buffers out of the function to avoid overflowing stack size
unsigned int scaled[512 * 512];
unsigned char paletted_texture[512 * 512];

//#define DEBUG_MIPMAPS

#ifdef DEBUG_MIPMAPS
unsigned int mipmap_debug_colors[] =
{
	(255 << 24) | 255,
	(255 << 24) | (255 << 8),
	(255 << 24) | (255 << 16),
	(255 << 24) | (255 << 8) | 255,
	(255 << 24) | (255 << 16) | (255 << 8),
	(255 << 24) | (255 << 24) | (255 << 16),
	(255 << 24) | (255 << 24) | (255 << 16) | (255 << 8),
	(255 << 24)
};
#endif

bool VK_Upload32(unsigned* data, image_t* image, int width, int height, bool mipmap)
{
	int			samples;
	int			scaled_width, scaled_height;
	int			i, c;
	byte* scan;
	int comp;

	if (vk_no_po2->value) //need to resample, npo2 textures
		//or you set down your texture quality for no good reason
	{
		for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
			;
		if (vk_round_down->value && scaled_width > width && mipmap)
			scaled_width >>= 1;
		for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
			;
		if (vk_round_down->value && scaled_height > height && mipmap)
			scaled_height >>= 1;

		// let people sample down the world textures for speed
		if (mipmap)
		{
			//bitshift for speed
			scaled_width >>= (int)(vk_picmip->value);
			scaled_height >>= (int)(vk_picmip->value);
		}
	}
	else //don't need to resample
	{
		scaled_width = width;
		scaled_height = height;

		//someone might actually want to downsample the texture quality for some reason
		if (mipmap)
		{
			scaled_width >>= (int)(vk_picmip->value);
			scaled_height >>= (int)(vk_picmip->value);
		}
	}

	// don't ever bother with >256 textures
	//[ISB] let's try 512 instead
	if (scaled_width > 512)
		scaled_width = 512;
	if (scaled_height > 512)
		scaled_height = 512;

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	//[ISB] the miplevel count must be determined before starting the upload. Ugh.
	image->miplevels = 1;
	image->layers = 1;
	if (mipmap)
	{
		int lscaled_width = scaled_width;
		int lscaled_height = scaled_height;
		int		totalmiplevel;

		totalmiplevel = 1;
		while (lscaled_width > 1 || lscaled_height > 1)
		{
			lscaled_width /= 2;
			lscaled_height /= 2;
			if (lscaled_width < 1)
				lscaled_width = 1;
			if (lscaled_height < 1)
				lscaled_height = 1;
			totalmiplevel++;
		}

		image->miplevels = totalmiplevel;
	}

	//All the data is known now, so finally create that damn image
	image->upload_width = scaled_width;
	image->upload_height = scaled_height;
	VK_CreateVKImageFromImage(image);
	VK_StartStagingImageData(&vk_state.device.stage, image);

	if (scaled_width * scaled_height > sizeof(scaled) / 4)
		ri.Sys_Error(ERR_DROP, "VK_Upload32: too big");

	// scan the texture for any non-255 alpha
	c = width * height;
	scan = ((byte*)data) + 3;
	samples = gl_solid_format;
	for (i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			samples = gl_alpha_format;
			break;
		}
	}

	if (samples == gl_solid_format)
		comp = gl_tex_solid_format;
	else if (samples == gl_alpha_format)
		comp = gl_tex_alpha_format;
	else 
	{
		ri.Con_Printf(PRINT_ALL,
			"Unknown number of texture components %i\n",
			samples);
		comp = samples;
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			VK_StageImageData(&vk_state.device.stage, image, 0, scaled_width, scaled_height, data);
			goto done;
		}
		memcpy(scaled, data, width * height * 4);
	}
	else
		VK_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);

	//VK_LightScaleTexture(scaled, scaled_width, scaled_height, !mipmap);

	VK_StageImageData(&vk_state.device.stage, image, 0, scaled_width, scaled_height, scaled);

	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			VK_MipMap((byte*)scaled, scaled_width, scaled_height);
			scaled_width /= 2;
			scaled_height /= 2;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;

#ifdef DEBUG_MIPMAPS
			for (int i = 0; i < scaled_width * scaled_height; i++)
			{
				scaled[i] = mipmap_debug_colors[miplevel & 7];
			}
#endif
			miplevel++;

			VK_StageImageData(&vk_state.device.stage, image, miplevel, scaled_width, scaled_height, scaled);
		}
	}
done:;

	VK_FinishStagingImage(&vk_state.device.stage, image);
	return (samples == gl_alpha_format);
}

unsigned	trans[512 * 256];

bool VK_Upload8(byte* data, image_t* image, int width, int height, bool mipmap, bool is_sky, unsigned int* palette)
{
	int			i, s;
	int			p;

	s = width * height;

	if (s > sizeof(trans) / 4)
		ri.Sys_Error(ERR_DROP, "VK_Upload8: too large");

	if (!palette)
		palette = d_8to24table;

	for (i = 0; i < s; i++)
	{
		p = data[i];
		trans[i] = palette[p];

		if (p == 255)
		{	// transparent, so scan around for another color
			// to avoid alpha fringes
			// FIXME: do a full flood fill so mips work...
			if (i > width && data[i - width] != 255)
				p = data[i - width];
			else if (i < s - width && data[i + width] != 255)
				p = data[i + width];
			else if (i > 0 && data[i - 1] != 255)
				p = data[i - 1];
			else if (i < s - 1 && data[i + 1] != 255)
				p = data[i + 1];
			else
				p = 0;
			// copy rgb components
			((byte*)&trans[i])[0] = ((byte*)&palette[p])[0];
			((byte*)&trans[i])[1] = ((byte*)&palette[p])[1];
			((byte*)&trans[i])[2] = ((byte*)&palette[p])[2];
		}
	}

	return VK_Upload32(trans, image, width, height, mipmap);
}

/*
================
VK_CreatePic

Creates a picture with a given vulkan format, and no default data. 
================
*/
image_t* VK_CreatePic(const char* name, int width, int height, int layers, VkFormat format, VkImageViewType viewformat, VkImageLayout destinationLayout)
{
	// find a free image_t
	image_t* image;
	int i;
	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (!image->handle)
			break;
	}
	if (i == numvktextures)
	{
		if (numvktextures == MAX_GLTEXTURES)
			ri.Sys_Error(ERR_DROP, "MAX_GLTEXTURES");
		numvktextures++;
	}
	image = &vktextures[i];

	if (strlen(name) >= sizeof(image->name))
		ri.Sys_Error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = image->upload_width = width;
	image->height = image->upload_height = height;
	image->miplevels = 1;
	image->layers = layers;
	image->type = it_pic;

	VkImageCreateInfo imageinfo = {};
	imageinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageinfo.format = format;
	imageinfo.extent.width = width;
	imageinfo.extent.height = height;
	imageinfo.extent.depth = 1;
	imageinfo.arrayLayers = layers;
	imageinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageinfo.mipLevels = 1;
	imageinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageinfo.imageType = VK_IMAGE_TYPE_2D;

	if (viewformat == VK_IMAGE_VIEW_TYPE_CUBE)
		imageinfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	//Initialize usage based on format or destination. This is a little dumb, but it will work for most images quake 2 uses
	if (destinationLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		imageinfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	else if (destinationLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		imageinfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	else
		imageinfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateImage(vk_state.device.handle, &imageinfo, NULL, &image->handle));

	VK_AssignImageName(image->handle, name);

	//The memory for an image must be bound before making the view
	VkMemoryRequirements reqs; vkGetImageMemoryRequirements(vk_state.device.handle, image->handle, &reqs);
	image->memory = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	vkBindImageMemory(vk_state.device.handle, image->handle, image->memory.memory, image->memory.offset);

	//Ugh! blegh! but this will work for all I need it to.
	if (format == VK_FORMAT_R16G16B16_UNORM)
		image->data_size = 6;
	else if (format == VK_FORMAT_R16G16B16A16_UNORM)
		image->data_size = 8;
	else
		image->data_size = 4;

	//Now create a view for the image
	VkImageViewCreateInfo viewinfo = {};
	viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewinfo.format = format;
	viewinfo.image = image->handle;
	viewinfo.viewType = viewformat;
	viewinfo.subresourceRange.layerCount = layers;
	viewinfo.subresourceRange.levelCount = 1;
	viewinfo.subresourceRange.baseArrayLayer = 0;
	viewinfo.subresourceRange.baseMipLevel = 0;
	viewinfo.subresourceRange.aspectMask = destinationLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	viewinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VK_CHECK(vkCreateImageView(vk_state.device.handle, &viewinfo, NULL, &image->viewhandle));

	VK_AssignViewName(image->viewhandle, name);

	//If the image is an attachment, transition it before usage. If it's not an attachment, data should be staged before usage. 
	if (destinationLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || destinationLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		//No data needs to be staged, but use the stage to insert the transition
		VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(&vk_state.device.stage);

		VkImageMemoryBarrier2 barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.image = image->handle;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = destinationLayout;
		barrier.srcAccessMask = VK_ACCESS_2_NONE;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.aspectMask = destinationLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		if (destinationLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		{
			barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		}
		else if (destinationLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else
		{
			//In quake 2, most images will simply be sampled from. 
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		}

		VkDependencyInfo depinfo = {};
		depinfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depinfo.imageMemoryBarrierCount = 1;
		depinfo.pImageMemoryBarriers = &barrier;

		vkCmdPipelineBarrier2(cmdbuffer, &depinfo);
	}
	else
	{
		//we're gonna be sampled from probably, so get us a descriptor set
		
		//Recycle the former descriptor if it exists
		if (image->descriptor.set == VK_NULL_HANDLE)
		{
			//Create the descriptor set now and update it
			VK_AllocateDescriptor(&vk_state.device.image_descriptor_pool, 1, &vk_state.device.sampled_texture_layout, &image->descriptor);
		}

		VkDescriptorImageInfo dimageinfo = {};
		dimageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dimageinfo.imageView = image->viewhandle;
		dimageinfo.sampler = GetSamplerForImage(image);

		VkWriteDescriptorSet writeinfo = {};
		writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeinfo.descriptorCount = 1;
		writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeinfo.dstBinding = 0;
		writeinfo.dstSet = image->descriptor.set;
		writeinfo.pImageInfo = &dimageinfo;

		vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);
	}

	image->sl = image->tl = 0.0f;
	image->sh = image->th = 1.0f;

	return image;
}

void VK_ChangeSampler(image_t* image, VkSampler sampler)
{
	//Recycle the former descriptor if it exists
	if (image->descriptor.set == VK_NULL_HANDLE)
	{
		//Create the descriptor set now and update it
		VK_AllocateDescriptor(&vk_state.device.image_descriptor_pool, 1, &vk_state.device.sampled_texture_layout, &image->descriptor);
	}

	VkDescriptorImageInfo dimageinfo = {};
	dimageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dimageinfo.imageView = image->viewhandle;
	dimageinfo.sampler = sampler;

	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = image->descriptor.set;
	writeinfo.pImageInfo = &dimageinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);
}

/*
================
VK_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t* VK_LoadPic(const char* name, byte* pic, int width, int height, imagetype_t type, int bits, unsigned int* palette)
{
	image_t* image;
	byte* paltemp = nullptr;
	int			i;

	if (!palette)
		palette = d_8to24table;

	//Test code, this should identify images by a hash. 
	if (!strcmp(name, "textures/e2u2/flesh1_1.wal"))
	{
		byte* pictemp;
		int widthtemp, heighttemp;

		LoadPCX("models/objects/r_explode/skin1.pcx", &pictemp, &paltemp, &widthtemp, &heighttemp);
		if (paltemp)
			palette = (unsigned int*)paltemp;

		if (pictemp)
			free(pictemp);
	}

	// find a free image_t
	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (!image->handle)
			break;
	}
	if (i == numvktextures)
	{
		if (numvktextures == MAX_GLTEXTURES)
			ri.Sys_Error(ERR_DROP, "MAX_GLTEXTURES");
		numvktextures++;
	}
	image = &vktextures[i];

	if (strlen(name) >= sizeof(image->name))
		ri.Sys_Error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if (type == it_skin && bits == 8)
		R_FloodFillSkin(pic, width, height);

	image->scrap = false;
	if (bits == 8)
		image->has_alpha = VK_Upload8(pic, image, width, height, (image->type != it_pic && image->type != it_sky), image->type == it_sky, palette);
	else
		image->has_alpha = VK_Upload32((unsigned*)pic, image, width, height, (image->type != it_pic && image->type != it_sky));

	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

	if (paltemp)
		free(paltemp);

	return image;
}

/*
================
VK_LoadWal
================
*/
image_t* VK_LoadWal(const char* name)
{
	miptex_t* mt;
	int			width, height, ofs;
	image_t* image;

	ri.FS_LoadFile(name, (void**)&mt);
	if (!mt)
	{
		ri.Con_Printf(PRINT_ALL, "VK_FindImage: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	image = VK_LoadPic(name, (byte*)mt + ofs, width, height, it_wall, 8);

	ri.FS_FreeFile((void*)mt);

	return image;
}

/*
===============
VK_FindImage

Finds or loads the given image
===============
*/
image_t* VK_FindImage(const char* name, imagetype_t type)
{
	image_t* image;
	int		i, len;
	byte* pic, * palette;
	int		width, height;

	if (!name)
		return nullptr;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: NULL name");
	len = strlen(name);
	if (len < 5)
		return nullptr;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad name: %s", name);

	// look for it
	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	pic = nullptr;
	palette = nullptr;
	if (!strcmp(name + len - 4, ".pcx"))
	{
		LoadPCX(name, &pic, &palette, &width, &height);

		//This PCX image has a palette, so use it. This fixes a problem with the BFG impact and some of the explosion textures having gray pixels. 
		//However, it may not be usable as is. Some images like the blaster's world model texture use a grayscale palette and are reliant on using 
		//the colormap's palette. Try to detect this case. 
		if (palette)
		{
			for (i = 0; i < 256; i++)
			{
				if (palette[i * 4] != i || palette[i * 4 + 1] != i || palette[i * 4 + 2] != i)
					break;
			}

			if (i == 256) //made it to the end?
			{
				free(palette);
				palette = nullptr;
			}
		}

		if (!pic)
			return nullptr; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);
		image = VK_LoadPic(name, pic, width, height, type, 8, (unsigned int*)palette);
	}
	else if (!strcmp(name + len - 4, ".wal"))
	{
		image = VK_LoadWal(name);
	}
	else if (!strcmp(name + len - 4, ".tga"))
	{
		LoadTGA(name, &pic, &width, &height);
		if (!pic)
			return NULL; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);
		image = VK_LoadPic(name, pic, width, height, type, 32);
	}
	else
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad extension on: %s", name);


	if (pic)
		free(pic);
	if (palette)
		free(palette);

	return image;
}

/*
===============
R_RegisterSkin
===============
*/
struct image_s* R_RegisterSkin(char* name)
{
	return VK_FindImage(name, it_skin);
}

/*
================
VK_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void VK_FreeUnusedImages(void)
{
	int		i;
	image_t* image;

	//A little brute force, but we're occasionally doing purges while something is in use. 
	VK_SyncFrame();

	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		if (image->type == it_pic)
			continue;		// don't free pics
		if (image->locked)
			continue;		// don't free locked pics
		// free it
		VK_FreeImage(image);
	}
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette(void)
{
	int		r, g, b;
	unsigned	v;
	byte* pic, * pal;
	int		width, height;

	// get the palette

	LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");
		return 0;
	}

	/*for (i = 0; i < 256; i++)
	{
		r = pal[i * 3 + 0];
		g = pal[i * 3 + 1];
		b = pal[i * 3 + 2];

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		d_8to24table[i] = LittleLong(v);
	}*/

	unsigned int* pallong = (unsigned int*)pal;
	for (int i = 0; i < 256; i++)
	{
		d_8to24table[i] = LittleLong(pallong[i]);
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	free(pic);
	free(pal);

	return 0;
}

/*
===============
VK_InitImages
===============
*/
void	VK_InitImages(void)
{
	int		i, j;
	float	g = vid_gamma->value;

	registration_sequence = 1;

	// init intensity conversions
	intensity = ri.Cvar_Get("intensity", "2", 0);

	if (intensity->value <= 1)
		ri.Cvar_Set("intensity", "1");

	vk_state.inverse_intensity = 1 / intensity->value;

	Draw_GetPalette();

	for (i = 0; i < 256; i++)
	{
		if (g == 1)
		{
			gammatable[i] = i;
		}
		else
		{
			float inf;

			inf = 255 * pow((i + 0.5) / 255.5, g) + 0.5;
			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;
			gammatable[i] = inf;
		}
	}

	for (i = 0; i < 256; i++)
	{
		j = i * intensity->value;
		if (j > 255)
			j = 255;
		intensitytable[i] = j;
	}

	VK_SetTextureMode();
}

/*
===============
VK_ShutdownImages
===============
*/
void	VK_ShutdownImages(void)
{
	int		i;
	image_t* image;

	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		VK_FreeImage(image);
		memset(image, 0, sizeof(*image));
	}
}

void VK_CreateSamplers(vklogicaldevice_t* device)
{
	VkSamplerCreateInfo samplerinfo = {};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.minFilter = VK_FILTER_NEAREST;
	samplerinfo.magFilter = VK_FILTER_NEAREST;
	samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VK_CHECK(vkCreateSampler(device->handle, &samplerinfo, NULL, &device->nearest_sampler));

	samplerinfo.minFilter = VK_FILTER_LINEAR;
	samplerinfo.magFilter = VK_FILTER_LINEAR;
	VK_CHECK(vkCreateSampler(device->handle, &samplerinfo, NULL, &device->linear_sampler));

	samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerinfo.maxLod = VK_LOD_CLAMP_NONE;
	VK_CHECK(vkCreateSampler(device->handle, &samplerinfo, NULL, &device->linear_mipmap_nearest_sampler));

	samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	VK_CHECK(vkCreateSampler(device->handle, &samplerinfo, NULL, &device->linear_mipmap_linear_sampler));

	samplerinfo.minFilter = VK_FILTER_NEAREST;
	samplerinfo.magFilter = VK_FILTER_NEAREST;
	samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	VK_CHECK(vkCreateSampler(device->handle, &samplerinfo, NULL, &device->nearest_mipmap_nearest_sampler));

	samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	VK_CHECK(vkCreateSampler(device->handle, &samplerinfo, NULL, &device->nearest_mipmap_linear_sampler));
}

void VK_DestroySamplers(vklogicaldevice_t* device)
{
	vkDestroySampler(device->handle, device->nearest_sampler, NULL);
	vkDestroySampler(device->handle, device->linear_sampler, NULL);
	vkDestroySampler(device->handle, device->nearest_mipmap_linear_sampler, NULL);
	vkDestroySampler(device->handle, device->nearest_mipmap_nearest_sampler, NULL);
	vkDestroySampler(device->handle, device->linear_mipmap_linear_sampler, NULL);
	vkDestroySampler(device->handle, device->linear_mipmap_nearest_sampler, NULL);
}

void VK_UpdateSamplers()
{
	VK_SetTextureMode();

	for (int i = 0; i < MAX_GLTEXTURES; i++)
	{
		image_t* image = &vktextures[i];
		if (!image->registration_sequence)
			continue;

		if (image->type == it_pic)
			continue;

		VK_ChangeSampler(image, GetSamplerForImage(image));
	}
}
