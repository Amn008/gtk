/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <math.h>
#include <string.h>
#include <cairo-gobject.h>

#include "gtkcssstylepropertyprivate.h"
#include "gtkiconhelperprivate.h"
#include "gtkimageprivate.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkimageaccessible.h"

/**
 * SECTION:gtkimage
 * @Short_description: A widget displaying an image
 * @Title: GtkImage
 * @See_also:#GdkPixbuf
 *
 * The #GtkImage widget displays an image. Various kinds of object
 * can be displayed as an image; most typically, you would load a
 * #GdkPixbuf ("pixel buffer") from a file, and then display that.
 * There’s a convenience function to do this, gtk_image_new_from_file(),
 * used as follows:
 * |[<!-- language="C" -->
 *   GtkWidget *image;
 *   image = gtk_image_new_from_file ("myfile.png");
 * ]|
 * If the file isn’t loaded successfully, the image will contain a
 * “broken image” icon similar to that used in many web browsers.
 * If you want to handle errors in loading the file yourself,
 * for example by displaying an error message, then load the image with
 * gdk_pixbuf_new_from_file(), then create the #GtkImage with
 * gtk_image_new_from_pixbuf().
 *
 * The image file may contain an animation, if so the #GtkImage will
 * display an animation (#GdkPixbufAnimation) instead of a static image.
 *
 * Sometimes an application will want to avoid depending on external data
 * files, such as image files. See the documentation of #GResource for details.
 * In this case, the #GtkImage:resource, gtk_image_new_from_resource() and
 * gtk_image_set_from_resource() should be used.
 *
 * # CSS nodes
 *
 * GtkImage has a single CSS node with the name image.
 */


struct _GtkImagePrivate
{
  GtkIconHelper icon_helper;

  GdkPixbufAnimationIter *animation_iter;
  gint animation_timeout;

  float baseline_align;

  gchar                *filename;       /* Only used with GTK_IMAGE_ANIMATION, GTK_IMAGE_PIXBUF */
  gchar                *resource_path;  /* Only used with GTK_IMAGE_PIXBUF */
};


#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_BUTTON
static void gtk_image_snapshot             (GtkWidget    *widget,
                                            GtkSnapshot  *snapshot);
static void gtk_image_size_allocate        (GtkWidget           *widget,
                                            const GtkAllocation *allocation,
                                            int                  baseline,
                                            GtkAllocation       *out_clip);
static void gtk_image_unmap                (GtkWidget    *widget);
static void gtk_image_unrealize            (GtkWidget    *widget);
static void gtk_image_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int            for_size,
                               int           *minimum,
                               int           *natural,
                               int           *minimum_baseline,
                               int           *natural_baseline);

static void gtk_image_style_updated        (GtkWidget    *widget);
static void gtk_image_finalize             (GObject      *object);

static void gtk_image_set_property         (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void gtk_image_get_property         (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

enum
{
  PROP_0,
  PROP_PIXBUF,
  PROP_SURFACE,
  PROP_FILE,
  PROP_ICON_SIZE,
  PROP_PIXEL_SIZE,
  PROP_PIXBUF_ANIMATION,
  PROP_ICON_NAME,
  PROP_STORAGE_TYPE,
  PROP_GICON,
  PROP_RESOURCE,
  PROP_USE_FALLBACK,
  NUM_PROPERTIES
};

static GParamSpec *image_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkImage, gtk_image, GTK_TYPE_WIDGET)

static void
gtk_image_class_init (GtkImageClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  
  gobject_class->set_property = gtk_image_set_property;
  gobject_class->get_property = gtk_image_get_property;
  gobject_class->finalize = gtk_image_finalize;

  widget_class = GTK_WIDGET_CLASS (class);
  widget_class->snapshot = gtk_image_snapshot;
  widget_class->measure = gtk_image_measure;
  widget_class->size_allocate = gtk_image_size_allocate;
  widget_class->unmap = gtk_image_unmap;
  widget_class->unrealize = gtk_image_unrealize;
  widget_class->style_updated = gtk_image_style_updated;

  image_props[PROP_PIXBUF] =
      g_param_spec_object ("pixbuf",
                           P_("Pixbuf"),
                           P_("A GdkPixbuf to display"),
                           GDK_TYPE_PIXBUF,
                           GTK_PARAM_READWRITE);

  image_props[PROP_SURFACE] =
      g_param_spec_boxed ("surface",
                          P_("Surface"),
                          P_("A cairo_surface_t to display"),
                          CAIRO_GOBJECT_TYPE_SURFACE,
                          GTK_PARAM_READWRITE);

  image_props[PROP_FILE] =
      g_param_spec_string ("file",
                           P_("Filename"),
                           P_("Filename to load and display"),
                           NULL,
                           GTK_PARAM_READWRITE);

  image_props[PROP_ICON_SIZE] =
      g_param_spec_int ("icon-size",
                        P_("Icon size"),
                        P_("Symbolic size to use for icon set or named icon"),
                        0, G_MAXINT,
                        DEFAULT_ICON_SIZE,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkImage:pixel-size:
   *
   * The "pixel-size" property can be used to specify a fixed size
   * overriding the #GtkImage:icon-size property for images of type
   * %GTK_IMAGE_ICON_NAME.
   *
   * Since: 2.6
   */
  image_props[PROP_PIXEL_SIZE] =
      g_param_spec_int ("pixel-size",
                        P_("Pixel size"),
                        P_("Pixel size to use for named icon"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  image_props[PROP_PIXBUF_ANIMATION] =
      g_param_spec_object ("pixbuf-animation",
                           P_("Animation"),
                           P_("GdkPixbufAnimation to display"),
                           GDK_TYPE_PIXBUF_ANIMATION,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:icon-name:
   *
   * The name of the icon in the icon theme. If the icon theme is
   * changed, the image will be updated automatically.
   *
   * Since: 2.6
   */
  image_props[PROP_ICON_NAME] =
      g_param_spec_string ("icon-name",
                           P_("Icon Name"),
                           P_("The name of the icon from the icon theme"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:gicon:
   *
   * The GIcon displayed in the GtkImage. For themed icons,
   * If the icon theme is changed, the image will be updated
   * automatically.
   *
   * Since: 2.14
   */
  image_props[PROP_GICON] =
      g_param_spec_object ("gicon",
                           P_("Icon"),
                           P_("The GIcon being displayed"),
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:resource:
   *
   * A path to a resource file to display.
   *
   * Since: 3.8
   */
  image_props[PROP_RESOURCE] =
      g_param_spec_string ("resource",
                           P_("Resource"),
                           P_("The resource path being displayed"),
                           NULL,
                           GTK_PARAM_READWRITE);

  image_props[PROP_STORAGE_TYPE] =
      g_param_spec_enum ("storage-type",
                         P_("Storage type"),
                         P_("The representation being used for image data"),
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkImage:use-fallback:
   *
   * Whether the icon displayed in the GtkImage will use
   * standard icon names fallback. The value of this property
   * is only relevant for images of type %GTK_IMAGE_ICON_NAME
   * and %GTK_IMAGE_GICON.
   *
   * Since: 3.0
   */
  image_props[PROP_USE_FALLBACK] =
      g_param_spec_boolean ("use-fallback",
                            P_("Use Fallback"),
                            P_("Whether to use icon names fallback"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, image_props);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_IMAGE_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "image");
}

static void
gtk_image_init (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkCssNode *widget_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (image));
  gtk_widget_set_has_window (GTK_WIDGET (image), FALSE);

  gtk_icon_helper_init (&priv->icon_helper, widget_node, GTK_WIDGET (image));
  _gtk_icon_helper_set_icon_size (&priv->icon_helper, DEFAULT_ICON_SIZE);
}

static void
gtk_image_finalize (GObject *object)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  gtk_icon_helper_destroy (&priv->icon_helper);

  g_free (priv->filename);
  g_free (priv->resource_path);

  G_OBJECT_CLASS (gtk_image_parent_class)->finalize (object);
};

static void
gtk_image_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkIconSize icon_size = _gtk_icon_helper_get_icon_size (&priv->icon_helper);

  if (icon_size == GTK_ICON_SIZE_INVALID)
    icon_size = DEFAULT_ICON_SIZE;

  switch (prop_id)
    {
    case PROP_PIXBUF:
      gtk_image_set_from_pixbuf (image, g_value_get_object (value));
      break;
    case PROP_SURFACE:
      gtk_image_set_from_surface (image, g_value_get_boxed (value));
      break;
    case PROP_FILE:
      gtk_image_set_from_file (image, g_value_get_string (value));
      break;
    case PROP_ICON_SIZE:
      if (_gtk_icon_helper_set_icon_size (&priv->icon_helper, g_value_get_int (value)))
        {
          g_object_notify_by_pspec (object, pspec);
          gtk_widget_queue_resize (GTK_WIDGET (image));
        }
      break;
    case PROP_PIXEL_SIZE:
      gtk_image_set_pixel_size (image, g_value_get_int (value));
      break;
    case PROP_PIXBUF_ANIMATION:
      gtk_image_set_from_animation (image, g_value_get_object (value));
      break;
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (image, g_value_get_string (value), icon_size);
      break;
    case PROP_GICON:
      gtk_image_set_from_gicon (image, g_value_get_object (value), icon_size);
      break;
    case PROP_RESOURCE:
      gtk_image_set_from_resource (image, g_value_get_string (value));
      break;

    case PROP_USE_FALLBACK:
      if (_gtk_icon_helper_set_use_fallback (&priv->icon_helper, g_value_get_boolean (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_image_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  switch (prop_id)
    {
    case PROP_PIXBUF:
      g_value_set_object (value, _gtk_icon_helper_peek_pixbuf (&priv->icon_helper));
      break;
    case PROP_SURFACE:
      g_value_set_boxed (value, _gtk_icon_helper_peek_surface (&priv->icon_helper));
      break;
    case PROP_FILE:
      g_value_set_string (value, priv->filename);
      break;
    case PROP_ICON_SIZE:
      g_value_set_int (value, _gtk_icon_helper_get_icon_size (&priv->icon_helper));
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_int (value, _gtk_icon_helper_get_pixel_size (&priv->icon_helper));
      break;
    case PROP_PIXBUF_ANIMATION:
      g_value_set_object (value, _gtk_icon_helper_peek_animation (&priv->icon_helper));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, _gtk_icon_helper_get_icon_name (&priv->icon_helper));
      break;
    case PROP_GICON:
      g_value_set_object (value, _gtk_icon_helper_peek_gicon (&priv->icon_helper));
      break;
    case PROP_RESOURCE:
      g_value_set_string (value, priv->resource_path);
      break;
    case PROP_USE_FALLBACK:
      g_value_set_boolean (value, _gtk_icon_helper_get_use_fallback (&priv->icon_helper));
      break;
    case PROP_STORAGE_TYPE:
      g_value_set_enum (value, _gtk_icon_helper_get_storage_type (&priv->icon_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * gtk_image_new_from_file:
 * @filename: (type filename): a filename
 * 
 * Creates a new #GtkImage displaying the file @filename. If the file
 * isn’t found or can’t be loaded, the resulting #GtkImage will
 * display a “broken image” icon. This function never returns %NULL,
 * it always returns a valid #GtkImage widget.
 *
 * If the file contains an animation, the image will contain an
 * animation.
 *
 * If you need to detect failures to load the file, use
 * gdk_pixbuf_new_from_file() to load the file yourself, then create
 * the #GtkImage from the pixbuf. (Or for animations, use
 * gdk_pixbuf_animation_new_from_file()).
 *
 * The storage type (gtk_image_get_storage_type()) of the returned
 * image is not defined, it will be whatever is appropriate for
 * displaying the file.
 * 
 * Returns: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_file   (const gchar *filename)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_file (image, filename);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_resource:
 * @resource_path: a resource path
 *
 * Creates a new #GtkImage displaying the resource file @resource_path. If the file
 * isn’t found or can’t be loaded, the resulting #GtkImage will
 * display a “broken image” icon. This function never returns %NULL,
 * it always returns a valid #GtkImage widget.
 *
 * If the file contains an animation, the image will contain an
 * animation.
 *
 * If you need to detect failures to load the file, use
 * gdk_pixbuf_new_from_file() to load the file yourself, then create
 * the #GtkImage from the pixbuf. (Or for animations, use
 * gdk_pixbuf_animation_new_from_file()).
 *
 * The storage type (gtk_image_get_storage_type()) of the returned
 * image is not defined, it will be whatever is appropriate for
 * displaying the file.
 *
 * Returns: a new #GtkImage
 *
 * Since: 3.4
 **/
GtkWidget*
gtk_image_new_from_resource (const gchar *resource_path)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_resource (image, resource_path);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_pixbuf:
 * @pixbuf: (allow-none): a #GdkPixbuf, or %NULL
 *
 * Creates a new #GtkImage displaying @pixbuf.
 * The #GtkImage does not assume a reference to the
 * pixbuf; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Note that this function just creates an #GtkImage from the pixbuf. The
 * #GtkImage created will not react to state changes. Should you want that, 
 * you should use gtk_image_new_from_icon_name().
 * 
 * Returns: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_pixbuf (image, pixbuf);

  return GTK_WIDGET (image);  
}

/**
 * gtk_image_new_from_surface:
 * @surface: (allow-none): a #cairo_surface_t, or %NULL
 *
 * Creates a new #GtkImage displaying @surface.
 * The #GtkImage does not assume a reference to the
 * surface; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Returns: a new #GtkImage
 *
 * Since: 3.10
 **/
GtkWidget*
gtk_image_new_from_surface (cairo_surface_t *surface)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_surface (image, surface);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_animation:
 * @animation: an animation
 * 
 * Creates a #GtkImage displaying the given animation.
 * The #GtkImage does not assume a reference to the
 * animation; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 *
 * Note that the animation frames are shown using a timeout with
 * #G_PRIORITY_DEFAULT. When using animations to indicate busyness,
 * keep in mind that the animation will only be shown if the main loop
 * is not busy with something that has a higher priority.
 *
 * Returns: a new #GtkImage widget
 **/
GtkWidget*
gtk_image_new_from_animation (GdkPixbufAnimation *animation)
{
  GtkImage *image;

  g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), NULL);
  
  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_animation (image, animation);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_icon_name:
 * @icon_name: (nullable): an icon name or %NULL
 * @size: (type int): a stock icon size (#GtkIconSize)
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 * 
 * Returns: a new #GtkImage displaying the themed icon
 *
 * Since: 2.6
 **/
GtkWidget*
gtk_image_new_from_icon_name (const gchar    *icon_name,
			      GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_icon_name (image, icon_name, size);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_gicon:
 * @icon: an icon
 * @size: (type int): a stock icon size (#GtkIconSize)
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 * 
 * Returns: a new #GtkImage displaying the themed icon
 *
 * Since: 2.14
 **/
GtkWidget*
gtk_image_new_from_gicon (GIcon *icon,
			  GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_gicon (image, icon, size);

  return GTK_WIDGET (image);
}

typedef struct {
  GtkImage *image;
  gint scale_factor;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
			 gint             width,
			 gint             height,
			 gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  gint scale_factor;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable images */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale_factor = 1;
      return;
    }

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (loader_data->image));
  gdk_pixbuf_loader_set_size (loader, width * scale_factor, height * scale_factor);
  loader_data->scale_factor = scale_factor;
}

static GdkPixbufAnimation *
load_scalable_with_loader (GtkImage    *image,
			   const gchar *file_path,
			   const gchar *resource_path,
			   gint        *scale_factor_out)
{
  GdkPixbufLoader *loader;
  GBytes *bytes;
  char *contents;
  gsize length;
  gboolean res;
  GdkPixbufAnimation *animation;
  LoaderData loader_data;

  animation = NULL;
  bytes = NULL;

  loader = gdk_pixbuf_loader_new ();
  loader_data.image = image;

  g_signal_connect (loader, "size-prepared", G_CALLBACK (on_loader_size_prepared), &loader_data);

  if (resource_path != NULL)
    {
      bytes = g_resources_lookup_data (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    }
  else if (file_path != NULL)
    {
      res = g_file_get_contents (file_path, &contents, &length, NULL);
      if (res)
	bytes = g_bytes_new_take (contents, length);
    }
  else
    {
      g_assert_not_reached ();
    }

  if (!bytes)
    goto out;

  if (!gdk_pixbuf_loader_write_bytes (loader, bytes, NULL))
    goto out;

  if (!gdk_pixbuf_loader_close (loader, NULL))
    goto out;

  animation = gdk_pixbuf_loader_get_animation (loader);
  if (animation != NULL)
    {
      g_object_ref (animation);
      if (scale_factor_out != NULL)
	*scale_factor_out = loader_data.scale_factor;
    }

 out:
  gdk_pixbuf_loader_close (loader, NULL);
  g_object_unref (loader);
  g_bytes_unref (bytes);

  return animation;
}

/**
 * gtk_image_set_from_file:
 * @image: a #GtkImage
 * @filename: (type filename) (allow-none): a filename or %NULL
 *
 * See gtk_image_new_from_file() for details.
 **/
void
gtk_image_set_from_file   (GtkImage    *image,
                           const gchar *filename)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GdkPixbufAnimation *anim;
  gint scale_factor;
  
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  
  gtk_image_clear (image);

  if (filename == NULL)
    {
      priv->filename = NULL;
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  anim = load_scalable_with_loader (image, filename, NULL, &scale_factor);

  if (anim == NULL)
    {
      gtk_image_set_from_icon_name (image,
                                    "image-missing",
                                    DEFAULT_ICON_SIZE);
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  /* We could just unconditionally set_from_animation,
   * but it's nicer for memory if we toss the animation
   * if it's just a single pixbuf
   */

  if (gdk_pixbuf_animation_is_static_image (anim))
    gtk_image_set_from_pixbuf (image,
			       gdk_pixbuf_animation_get_static_image (anim));
  else
    gtk_image_set_from_animation (image, anim);

  _gtk_icon_helper_set_pixbuf_scale (&priv->icon_helper, scale_factor);

  g_object_unref (anim);

  priv->filename = g_strdup (filename);
  
  g_object_thaw_notify (G_OBJECT (image));
}

#ifndef GDK_PIXBUF_MAGIC_NUMBER
#define GDK_PIXBUF_MAGIC_NUMBER (0x47646b50)    /* 'GdkP' */
#endif

static gboolean
resource_is_pixdata (const gchar *resource_path)
{
  const guint8 *stream;
  guint32 magic;
  gsize data_size;
  GBytes *bytes;
  gboolean ret = FALSE;

  bytes = g_resources_lookup_data (resource_path, 0, NULL);
  if (bytes == NULL)
    return FALSE;

  stream = g_bytes_get_data (bytes, &data_size);
  if (data_size < sizeof(guint32))
    goto out;

  magic = (stream[0] << 24) + (stream[1] << 16) + (stream[2] << 8) + stream[3];
  if (magic == GDK_PIXBUF_MAGIC_NUMBER)
    ret = TRUE;

out:
  g_bytes_unref (bytes);
  return ret;
}

/**
 * gtk_image_set_from_resource:
 * @image: a #GtkImage
 * @resource_path: (allow-none): a resource path or %NULL
 *
 * See gtk_image_new_from_resource() for details.
 **/
void
gtk_image_set_from_resource (GtkImage    *image,
			     const gchar *resource_path)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GdkPixbufAnimation *animation;
  gint scale_factor = 1;

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (resource_path == NULL)
    {
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  if (resource_is_pixdata (resource_path))
    {
      g_warning ("GdkPixdata format images are not supported, remove the \"to-pixdata\" option from your GResource files");
      animation = NULL;
    }
  else
    {
      animation = load_scalable_with_loader (image, NULL, resource_path, &scale_factor);
    }

  if (animation == NULL)
    {
      gtk_image_set_from_icon_name (image,
                                    "image-missing",
                                    DEFAULT_ICON_SIZE);
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  if (gdk_pixbuf_animation_is_static_image (animation))
    gtk_image_set_from_pixbuf (image, gdk_pixbuf_animation_get_static_image (animation));
  else
    gtk_image_set_from_animation (image, animation);

  _gtk_icon_helper_set_pixbuf_scale (&priv->icon_helper, scale_factor);

  priv->resource_path = g_strdup (resource_path);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_RESOURCE]);

  g_object_unref (animation);

  g_object_thaw_notify (G_OBJECT (image));
}


/**
 * gtk_image_set_from_pixbuf:
 * @image: a #GtkImage
 * @pixbuf: (allow-none): a #GdkPixbuf or %NULL
 *
 * See gtk_image_new_from_pixbuf() for details.
 **/
void
gtk_image_set_from_pixbuf (GtkImage  *image,
                           GdkPixbuf *pixbuf)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixbuf == NULL ||
                    GDK_IS_PIXBUF (pixbuf));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (pixbuf != NULL)
    _gtk_icon_helper_set_pixbuf (&priv->icon_helper, pixbuf);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXBUF]);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_animation:
 * @image: a #GtkImage
 * @animation: the #GdkPixbufAnimation
 * 
 * Causes the #GtkImage to display the given animation (or display
 * nothing, if you set the animation to %NULL).
 **/
void
gtk_image_set_from_animation (GtkImage           *image,
                              GdkPixbufAnimation *animation)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (animation == NULL ||
                    GDK_IS_PIXBUF_ANIMATION (animation));

  g_object_freeze_notify (G_OBJECT (image));
  
  if (animation)
    g_object_ref (animation);

  gtk_image_clear (image);

  if (animation != NULL)
    {
      _gtk_icon_helper_set_animation (&priv->icon_helper, animation);
      g_object_unref (animation);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXBUF_ANIMATION]);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_icon_name:
 * @image: a #GtkImage
 * @icon_name: (nullable): an icon name or %NULL
 * @size: (type int): an icon size (#GtkIconSize)
 *
 * See gtk_image_new_from_icon_name() for details.
 * 
 * Since: 2.6
 **/
void
gtk_image_set_from_icon_name  (GtkImage       *image,
			       const gchar    *icon_name,
			       GtkIconSize     size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (icon_name)
    _gtk_icon_helper_set_icon_name (&priv->icon_helper, icon_name, size);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_NAME]);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_SIZE]);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_gicon:
 * @image: a #GtkImage
 * @icon: an icon
 * @size: (type int): an icon size (#GtkIconSize)
 *
 * See gtk_image_new_from_gicon() for details.
 * 
 * Since: 2.14
 **/
void
gtk_image_set_from_gicon  (GtkImage       *image,
			   GIcon          *icon,
			   GtkIconSize     size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  if (icon)
    g_object_ref (icon);

  gtk_image_clear (image);

  if (icon)
    {
      _gtk_icon_helper_set_gicon (&priv->icon_helper, icon, size);
      g_object_unref (icon);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_GICON]);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_SIZE]);
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_surface:
 * @image: a #GtkImage
 * @surface: (nullable): a cairo_surface_t or %NULL
 *
 * See gtk_image_new_from_surface() for details.
 * 
 * Since: 3.10
 **/
void
gtk_image_set_from_surface (GtkImage       *image,
			    cairo_surface_t *surface)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  if (surface)
    cairo_surface_reference (surface);

  gtk_image_clear (image);

  if (surface)
    {
      _gtk_icon_helper_set_surface (&priv->icon_helper, surface);
      cairo_surface_destroy (surface);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_SURFACE]);
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_get_storage_type:
 * @image: a #GtkImage
 * 
 * Gets the type of representation being used by the #GtkImage
 * to store image data. If the #GtkImage has no image data,
 * the return value will be %GTK_IMAGE_EMPTY.
 * 
 * Returns: image representation being used
 **/
GtkImageType
gtk_image_get_storage_type (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_IMAGE_EMPTY);

  return _gtk_icon_helper_get_storage_type (&priv->icon_helper);
}

/**
 * gtk_image_get_pixbuf:
 * @image: a #GtkImage
 *
 * Gets the #GdkPixbuf being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PIXBUF (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned pixbuf.
 * 
 * Returns: (nullable) (transfer none): the displayed pixbuf, or %NULL if
 * the image is empty
 **/
GdkPixbuf*
gtk_image_get_pixbuf (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_pixbuf (&priv->icon_helper);
}

/**
 * gtk_image_get_animation:
 * @image: a #GtkImage
 *
 * Gets the #GdkPixbufAnimation being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ANIMATION (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned animation.
 * 
 * Returns: (nullable) (transfer none): the displayed animation, or %NULL if
 * the image is empty
 **/
GdkPixbufAnimation*
gtk_image_get_animation (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_animation (&priv->icon_helper);
}

/**
 * gtk_image_get_icon_name:
 * @image: a #GtkImage
 * @icon_name: (out) (transfer none) (allow-none): place to store an
 *     icon name, or %NULL
 * @size: (out) (allow-none) (type int): place to store an icon size
 *     (#GtkIconSize), or %NULL
 *
 * Gets the icon name and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_NAME (see gtk_image_get_storage_type()).
 * The returned string is owned by the #GtkImage and should not
 * be freed.
 * 
 * Since: 2.6
 **/
void
gtk_image_get_icon_name  (GtkImage     *image,
			  const gchar **icon_name,
			  GtkIconSize  *size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  if (icon_name)
    *icon_name = _gtk_icon_helper_get_icon_name (&priv->icon_helper);

  if (size)
    *size = _gtk_icon_helper_get_icon_size (&priv->icon_helper);
}

/**
 * gtk_image_get_gicon:
 * @image: a #GtkImage
 * @gicon: (out) (transfer none) (allow-none): place to store a
 *     #GIcon, or %NULL
 * @size: (out) (allow-none) (type int): place to store an icon size
 *     (#GtkIconSize), or %NULL
 *
 * Gets the #GIcon and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_GICON (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned #GIcon.
 * 
 * Since: 2.14
 **/
void
gtk_image_get_gicon (GtkImage     *image,
		     GIcon       **gicon,
		     GtkIconSize  *size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  if (gicon)
    *gicon = _gtk_icon_helper_peek_gicon (&priv->icon_helper);

  if (size)
    *size = _gtk_icon_helper_get_icon_size (&priv->icon_helper);
}

/**
 * gtk_image_new:
 * 
 * Creates a new empty #GtkImage widget.
 * 
 * Returns: a newly created #GtkImage widget. 
 **/
GtkWidget*
gtk_image_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE, NULL);
}

static void
gtk_image_reset_anim_iter (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  if (gtk_image_get_storage_type (image) == GTK_IMAGE_ANIMATION)
    {
      /* Reset the animation */
      if (priv->animation_timeout)
        {
          g_source_remove (priv->animation_timeout);
          priv->animation_timeout = 0;
        }

      g_clear_object (&priv->animation_iter);
    }
}

static void
gtk_image_size_allocate (GtkWidget           *widget,
                         const GtkAllocation *allocation,
                         int                  baseline,
                         GtkAllocation       *out_clip)
{
  _gtk_style_context_get_icon_extents (gtk_widget_get_style_context (widget),
                                       out_clip,
                                       allocation->x,
                                       allocation->y,
                                       allocation->width,
                                       allocation->height);
}

static void
gtk_image_unmap (GtkWidget *widget)
{
  gtk_image_reset_anim_iter (GTK_IMAGE (widget));

  GTK_WIDGET_CLASS (gtk_image_parent_class)->unmap (widget);
}

static void
gtk_image_unrealize (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  gtk_image_reset_anim_iter (image);

  gtk_icon_helper_invalidate (&priv->icon_helper);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->unrealize (widget);
}

static gint
animation_timeout (gpointer data)
{
  GtkImage *image = GTK_IMAGE (data);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  int delay;

  priv->animation_timeout = 0;

  gdk_pixbuf_animation_iter_advance (priv->animation_iter, NULL);

  delay = gdk_pixbuf_animation_iter_get_delay_time (priv->animation_iter);
  if (delay >= 0)
    {
      GtkWidget *widget = GTK_WIDGET (image);

      priv->animation_timeout =
        gdk_threads_add_timeout (delay, animation_timeout, image);
      g_source_set_name_by_id (priv->animation_timeout, "[gtk+] animation_timeout");

      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static GdkPixbuf *
get_animation_frame (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  if (priv->animation_iter == NULL)
    {
      int delay;

      priv->animation_iter = 
        gdk_pixbuf_animation_get_iter (_gtk_icon_helper_peek_animation (&priv->icon_helper), NULL);

      delay = gdk_pixbuf_animation_iter_get_delay_time (priv->animation_iter);
      if (delay >= 0) {
        priv->animation_timeout =
          gdk_threads_add_timeout (delay, animation_timeout, image);
        g_source_set_name_by_id (priv->animation_timeout, "[gtk+] animation_timeout");
      }
    }

  /* don't advance the anim iter here, or we could get frame changes between two
   * exposes of different areas.
   */
  return g_object_ref (gdk_pixbuf_animation_iter_get_pixbuf (priv->animation_iter));
}

static float
gtk_image_get_baseline_align (GtkImage *image)
{
  PangoContext *pango_context;
  PangoFontMetrics *metrics;
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);


  if (priv->baseline_align == 0.0)
    {
      pango_context = gtk_widget_get_pango_context (GTK_WIDGET (image));
      metrics = pango_context_get_metrics (pango_context,
					   pango_context_get_font_description (pango_context),
					   pango_context_get_language (pango_context));
      priv->baseline_align =
                (float)pango_font_metrics_get_ascent (metrics) /
                (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));

      pango_font_metrics_unref (metrics);
    }

  return priv->baseline_align;
}

static void
gtk_image_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  int x, y, width, height;
  gint w, h, baseline;

  gtk_widget_get_content_size (widget, &width, &height);
  x = 0;
  y = 0;

  _gtk_icon_helper_get_size (&priv->icon_helper, &w, &h);

  baseline = gtk_widget_get_allocated_baseline (widget);

  if (baseline == -1)
    y += floor(height - h) / 2;
  else
    y += CLAMP (baseline - h * gtk_image_get_baseline_align (image), 0, height - h);

  x += (width - w) / 2;

  if (gtk_image_get_storage_type (image) == GTK_IMAGE_ANIMATION)
    {
      GtkStyleContext *context = gtk_widget_get_style_context (widget);
      GdkPixbuf *pixbuf = get_animation_frame (image);

      gtk_snapshot_render_icon (snapshot, context, pixbuf, x, y);

      g_object_unref (pixbuf);
    }
  else
    {
      gtk_snapshot_offset (snapshot, x, y);
      gtk_icon_helper_snapshot (&priv->icon_helper, snapshot);
      gtk_snapshot_offset (snapshot, -x, -y);
    }
}

static void
gtk_image_notify_for_storage_type (GtkImage     *image,
                                   GtkImageType  storage_type)
{
  switch (storage_type)
    {
    case GTK_IMAGE_PIXBUF:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXBUF]);
      break;
    case GTK_IMAGE_ANIMATION:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXBUF_ANIMATION]);
      break;
    case GTK_IMAGE_ICON_NAME:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_NAME]);
      break;
    case GTK_IMAGE_GICON:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_GICON]);
      break;
    case GTK_IMAGE_SURFACE:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_SURFACE]);
      break;
    case GTK_IMAGE_EMPTY:
    default:
      break;
    }
}

void
gtk_image_set_from_definition (GtkImage           *image,
                               GtkImageDefinition *def,
                               GtkIconSize         icon_size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  
  gtk_image_clear (image);

  if (def != NULL)
    {
      _gtk_icon_helper_set_definition (&priv->icon_helper, def);

      gtk_image_notify_for_storage_type (image, gtk_image_definition_get_storage_type (def));
    }

  _gtk_icon_helper_set_icon_size (&priv->icon_helper, icon_size);
  
  g_object_thaw_notify (G_OBJECT (image));
}

GtkImageDefinition *
gtk_image_get_definition (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  return gtk_icon_helper_get_definition (&priv->icon_helper);
}

/**
 * gtk_image_clear:
 * @image: a #GtkImage
 *
 * Resets the image to be empty.
 *
 * Since: 2.8
 */
void
gtk_image_clear (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkImageType storage_type;

  g_object_freeze_notify (G_OBJECT (image));
  storage_type = gtk_image_get_storage_type (image);

  if (storage_type != GTK_IMAGE_EMPTY)
    g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_STORAGE_TYPE]);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_SIZE]);

  gtk_image_reset_anim_iter (image);

  gtk_image_notify_for_storage_type (image, storage_type);

  if (priv->filename)
    {
      g_free (priv->filename);
      priv->filename = NULL;
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_FILE]);
    }

  if (priv->resource_path)
    {
      g_free (priv->resource_path);
      priv->resource_path = NULL;
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_RESOURCE]);
    }

  _gtk_icon_helper_clear (&priv->icon_helper);

  g_object_thaw_notify (G_OBJECT (image));
}

static void
gtk_image_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int            for_size,
                   int           *minimum,
                   int           *natural,
                   int           *minimum_baseline,
                   int           *natural_baseline)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (GTK_IMAGE (widget));
  gint width, height;
  float baseline_align;

  _gtk_icon_helper_get_size (&priv->icon_helper, &width, &height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = *natural = width;
    }
  else
    {
      baseline_align = gtk_image_get_baseline_align (GTK_IMAGE (widget));
      *minimum = *natural = height;
      if (minimum_baseline)
        *minimum_baseline = height * baseline_align;
      if (natural_baseline)
        *natural_baseline = height * baseline_align;
    }
}

static void
gtk_image_style_updated (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkCssStyleChange *change = gtk_style_context_get_change (context);

  gtk_icon_helper_invalidate_for_change (&priv->icon_helper, change);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->style_updated (widget);

  priv->baseline_align = 0.0;
}

/**
 * gtk_image_set_pixel_size:
 * @image: a #GtkImage
 * @pixel_size: the new pixel size
 * 
 * Sets the pixel size to use for named icons. If the pixel size is set
 * to a value != -1, it is used instead of the icon size set by
 * gtk_image_set_from_icon_name().
 *
 * Since: 2.6
 */
void 
gtk_image_set_pixel_size (GtkImage *image,
			  gint      pixel_size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  if (_gtk_icon_helper_set_pixel_size (&priv->icon_helper, pixel_size))
    {
      if (gtk_widget_get_visible (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXEL_SIZE]);
    }
}

/**
 * gtk_image_get_pixel_size:
 * @image: a #GtkImage
 * 
 * Gets the pixel size used for named icons.
 *
 * Returns: the pixel size used for named icons.
 *
 * Since: 2.6
 */
gint
gtk_image_get_pixel_size (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), -1);

  return _gtk_icon_helper_get_pixel_size (&priv->icon_helper);
}
