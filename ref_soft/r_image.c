/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "r_local.h"
#include "../ref_common/r_image_common.h"

#define	MAX_RIMAGES	1024
image_t		r_images[MAX_RIMAGES];
int			numr_images;


/*
===============
R_ImageList_f
===============
*/
void	R_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;

	ri.Con_Printf (PRINT_ALL, "------------------\n");
	texels = 0;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (image->registration_sequence <= 0)
			continue;
		texels += image->width*image->height;
		switch (image->type)
		{
		case it_skin:
			ri.Con_Printf (PRINT_ALL, "M");
			break;
		case it_sprite:
			ri.Con_Printf (PRINT_ALL, "S");
			break;
		case it_wall:
			ri.Con_Printf (PRINT_ALL, "W");
			break;
		case it_pic:
			ri.Con_Printf (PRINT_ALL, "P");
			break;
		default:
			ri.Con_Printf (PRINT_ALL, " ");
			break;
		}

		ri.Con_Printf (PRINT_ALL,  " %3i %3i : %s\n",
			image->width, image->height, image->name);
	}
	ri.Con_Printf (PRINT_ALL, "Total texel count: %i\n", texels);
}

//=======================================================

image_t *R_FindFreeImage (void)
{
	image_t		*image;
	int			i;

	// find a free image_t
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (!image->registration_sequence)
			break;
	}
	if (i == numr_images)
	{
		if (numr_images == MAX_RIMAGES)
			ri.Sys_Error (ERR_DROP, "MAX_RIMAGES");
		numr_images++;
	}
	image = &r_images[i];

	return image;
}

/*
================
SWR_LoadPic

================
*/
static image_t *SWR_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type)
{
	image_t		*image;
	int			i, c, b;

	image = R_FindFreeImage ();
	if (strlen(name) >= sizeof(image->name))
		ri.Sys_Error (ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	c = width*height;
	image->pixels[0] = malloc (c);
	image->transparent = false;
	for (i=0 ; i<c ; i++)
	{
		b = pic[i];
		if (b == 255)
			image->transparent = true;
		image->pixels[0][i] = b;
	}

	return image;
}

/*
================
R_LoadWal
================
*/
image_t *R_LoadWal (char *name)
{
	miptex_t	*mt;
	int			ofs;
	image_t		*image;
	int			size;

	ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		ri.Con_Printf (PRINT_ALL, "R_LoadWal: can't load %s\n", name);
		return r_notexture_mip;
	}

	image = R_FindFreeImage ();
	strcpy (image->name, name);
	image->width = LittleLong (mt->width);
	image->height = LittleLong (mt->height);
	image->type = it_wall;
	image->registration_sequence = registration_sequence;

	size = image->width*image->height * (256+64+16+4)/256;
	image->pixels[0] = malloc (size);
	image->pixels[1] = image->pixels[0] + image->width*image->height;
	image->pixels[2] = image->pixels[1] + image->width*image->height/4;
	image->pixels[3] = image->pixels[2] + image->width*image->height/16;

	ofs = LittleLong (mt->offsets[0]);
	memcpy ( image->pixels[0], (byte *)mt + ofs, size);

	ri.FS_FreeFile ((void *)mt);

	return image;
}


/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len;
	byte	*pic, *palette;
	int		width, height;

	if (!name)
		return NULL;	// ri.Sys_Error (ERR_DROP, "R_FindImage: NULL name");
	len = strlen(name);
	if (len<5)
		return NULL;	// ri.Sys_Error (ERR_DROP, "R_FindImage: bad name: %s", name);

	// look for it
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
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
	pic = NULL;
	palette = NULL;
	if (!strcmp(name+len-4, ".pcx"))
	{
		LoadPCX (name, &pic, &palette, &width, &height);
		if (!pic)
			return NULL;	// ri.Sys_Error (ERR_DROP, "R_FindImage: can't load %s", name);
		image = SWR_LoadPic (name, pic, width, height, type);
	}
	else if (!strcmp(name+len-4, ".wal"))
	{
		image = R_LoadWal (name);
	}
	else if (!strcmp(name+len-4, ".tga"))
		return NULL;	// ri.Sys_Error (ERR_DROP, "R_FindImage: can't load %s in software renderer", name);
	else
		return NULL;	// ri.Sys_Error (ERR_DROP, "R_FindImage: bad extension on: %s", name);

	if (pic)
		free(pic);
	if (palette)
		free(palette);

	return image;
}



/*
===============
SWR_RegisterSkin
===============
*/
struct image_s *SWR_RegisterSkin (char *name)
{
	return R_FindImage (name, it_skin);
}


/*
================
R_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void R_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			Com_PageInMemory ((byte *)image->pixels[0], image->width*image->height);
			continue;		// used this sequence
		}
		if (!image->registration_sequence)
			continue;		// free texture
		if (image->type == it_pic)
			continue;		// don't free pics
		// free it
		free (image->pixels[0]);	// the other mip levels just follow
		memset (image, 0, sizeof(*image));
	}
}



/*
===============
R_InitImages
===============
*/
void	R_InitImages (void)
{
	registration_sequence = 1;
}

/*
===============
R_ShutdownImages
===============
*/
void	R_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free texture
		// free it
		free (image->pixels[0]);	// the other mip levels just follow
		memset (image, 0, sizeof(*image));
	}
}

