/**
 * Add a new thumb object to the parent.
 *
 * @param parent The parent object.
 * @return The new object or NULL if it cannot be created.
 *
 * @see elm_thumb_file_set()
 * @see elm_thumb_ethumb_client_get()
 *
 * @ingroup Elm_Thumb
 */
EAPI Evas_Object                *elm_thumb_add(Evas_Object *parent);

/**
 *
 * Set the file that will be used as thumbnail @b source.
 *
 * The file can be an image or a video (in that case, acceptable
 * extensions are: avi, mp4, ogv, mov, mpg and wmv). To start the
 * video animation, use the function elm_thumb_animate().
 *
 * @see elm_thumb_file_get()
 * @see elm_thumb_reload()
 * @see elm_thumb_animate()
 *
 * @ingroup Elm_Thumb
 *
 * @param[in] file The path to file that will be used as thumbnail source.
 * @param[in] key The key used in case of an EET file.
 */
EAPI void elm_thumb_file_set(Eo *obj, const char *file, const char *key);

/**
 *
 * Get the image or video path and key used to generate the thumbnail.
 *
 * @see elm_thumb_file_set()
 * @see elm_thumb_path_get()
 *
 * @ingroup Elm_Thumb
 *
 * @param[out] file The path to file that will be used as thumbnail source.
 * @param[out] key The key used in case of an EET file.
 */
EAPI void elm_thumb_file_get(const Eo *obj, const char **file, const char **key);

/**
 * @brief Make the thumbnail 'editable'.
 *
 * This means the thumbnail is a valid drag target for drag and drop, and can
 * be cut or pasted too.
 *
 * @param[in] edit The editable state, default is @c false.
 *
 * @ingroup Elm_Thumb
 */
EAPI Eina_Bool elm_thumb_editable_set(Evas_Object *obj, Eina_Bool edit);

/**
 * @brief Get whether the thumbnail is editable.
 *
 * This means the thumbnail is a valid drag target for drag and drop, and can
 * be cut or pasted too.
 *
 * @return The editable state, default is @c false.
 *
 * @ingroup Elm_Thumb
 */
EAPI Eina_Bool elm_thumb_editable_get(const Evas_Object *obj);
#include "elm_thumb.eo.legacy.h"