/*
 * delete_image.c
 */

/*
 * Copyright (C) 2012, 2013 Eric Biggers
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option) any
 * later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; if not, see http://www.gnu.org/licenses/.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "wimlib.h"
#include "wimlib/error.h"
#include "wimlib/metadata.h"
#include "wimlib/util.h"
#include "wimlib/wim.h"
#include "wimlib/xml.h"

/* API function documented in wimlib.h  */
WIMLIBAPI int
wimlib_delete_image(WIMStruct *wim, int image)
{
	int ret;
	int first, last;

	if (image == WIMLIB_ALL_IMAGES) {
		last = wim->hdr.image_count;
		first = 1;
	} else {
		last = image;
		first = image;
	}

	for (image = last; image >= first; image--) {
		DEBUG("Deleting image %d", image);

		/* Even if the dentry tree is not allocated, we must select it
		 * (and therefore allocate it) so that we can decrement stream
		 * reference counts.  */
		ret = select_wim_image(wim, image);
		if (ret)
			return ret;

		/* Unless the image metadata is shared by another WIMStruct,
		 * free the dentry tree, free the security data, and decrement
		 * stream reference counts.  */
		put_image_metadata(wim->image_metadata[image - 1], wim->lookup_table);

		/* Get rid of the empty slot in the image metadata array. */
		for (int i = image - 1; i < wim->hdr.image_count - 1; i++)
			wim->image_metadata[i] = wim->image_metadata[i + 1];

		/* Decrement the image count. */
		--wim->hdr.image_count;

		/* Fix the boot index. */
		if (wim->hdr.boot_idx == image)
			wim->hdr.boot_idx = 0;
		else if (wim->hdr.boot_idx > image)
			wim->hdr.boot_idx--;

		wim->current_image = WIMLIB_NO_IMAGE;

		/* Remove the image from the XML information. */
		xml_delete_image(&wim->wim_info, image);

		wim->deletion_occurred = 1;
	}
	return 0;
}
