/**
 * Add a new progress bar widget to the given parent Elementary
 * (container) object
 *
 * @param parent The parent object
 * @return a new progress bar widget handle or @c NULL, on errors
 *
 * This function inserts a new progress bar widget on the canvas.
 *
 * @ingroup Elm_Progressbar
 */
EAPI Evas_Object                 *elm_progressbar_add(Evas_Object *parent);

/**
 * @brief Control the (exact) length of the bar region of a given progress bar
 * widget
 *
 * This sets the minimum width (when in horizontal mode) or height (when in
 * vertical mode) of the actual bar area of the progress bar @c obj. This in
 * turn affects the object's minimum size. Use this when you're not setting
 * other size hints expanding on the given direction (like weight and alignment
 * hints) and you would like it to have a specific size.
 *
 * @note Icon, label and unit text around @c obj will require their own space,
 * which will make @c obj to require more the @c size, actually.
 *
 * @param[in] size The length of the progress bar's bar region
 *
 * @ingroup Elm_Progressbar
 */
EAPI void elm_progressbar_span_size_set(Evas_Object *obj, Evas_Coord size);

/**
 * @brief Control the (exact) length of the bar region of a given progress bar
 * widget
 *
 * This sets the minimum width (when in horizontal mode) or height (when in
 * vertical mode) of the actual bar area of the progress bar @c obj. This in
 * turn affects the object's minimum size. Use this when you're not setting
 * other size hints expanding on the given direction (like weight and alignment
 * hints) and you would like it to have a specific size.
 *
 * @note Icon, label and unit text around @c obj will require their own space,
 * which will make @c obj to require more the @c size, actually.
 *
 * @return The length of the progress bar's bar region
 *
 * @ingroup Elm_Progressbar
 */
EAPI Evas_Coord elm_progressbar_span_size_get(const Evas_Object *obj);

/**
 * @brief Control the progress value (in percentage) on a given progress bar
 * widget
 *
 * Use this call to set progress bar levels.
 *
 * @note If you passes a value out of the specified range for @c val, it will
 * be interpreted as the closest of the boundary values in the range.
 *
 * @param[in] val The progress value (must be between $0.0 and 1.0)
 *
 * @ingroup Elm_Progressbar
 */
EAPI void elm_progressbar_value_set(Evas_Object *obj, double val);

/**
 * @brief Control the progress value (in percentage) on a given progress bar
 * widget
 *
 * Use this call to set progress bar levels.
 *
 * @note If you passes a value out of the specified range for @c val, it will
 * be interpreted as the closest of the boundary values in the range.
 *
 * @return The progress value (must be between $0.0 and 1.0)
 *
 * @ingroup Elm_Progressbar
 */
EAPI double elm_progressbar_value_get(const Evas_Object *obj);

/**
 * @brief Invert a given progress bar widget's displaying values order
 *
 * A progress bar may be inverted, in which state it gets its values inverted,
 * with high values being on the left or top and low values on the right or
 * bottom, as opposed to normally have the low values on the former and high
 * values on the latter, respectively, for horizontal and vertical modes.
 *
 * @param[in] inverted Use @c true to make @c obj inverted, @c false to bring
 * it back to default, non-inverted values.
 *
 * @ingroup Elm_Progressbar
 */
EAPI void elm_progressbar_inverted_set(Evas_Object *obj, Eina_Bool inverted);

/**
 * @brief Invert a given progress bar widget's displaying values order
 *
 * A progress bar may be inverted, in which state it gets its values inverted,
 * with high values being on the left or top and low values on the right or
 * bottom, as opposed to normally have the low values on the former and high
 * values on the latter, respectively, for horizontal and vertical modes.
 *
 * @return Use @c true to make @c obj inverted, @c false to bring it back to
 * default, non-inverted values.
 *
 * @ingroup Elm_Progressbar
 */
EAPI Eina_Bool elm_progressbar_inverted_get(const Evas_Object *obj);

/**
 * @brief Control the orientation of a given progress bar widget
 *
 * Use this function to change how your progress bar is to be disposed:
 * vertically or horizontally.
 *
 * @param[in] horizontal Use @c true to make @c obj to be horizontal, @c false
 * to make it vertical
 *
 * @ingroup Elm_Progressbar
 */
EAPI void elm_progressbar_horizontal_set(Evas_Object *obj, Eina_Bool horizontal);

/**
 * @brief Control the orientation of a given progress bar widget
 *
 * Use this function to change how your progress bar is to be disposed:
 * vertically or horizontally.
 *
 * @return Use @c true to make @c obj to be horizontal, @c false to make it
 * vertical
 *
 * @ingroup Elm_Progressbar
 */
EAPI Eina_Bool elm_progressbar_horizontal_get(const Evas_Object *obj);

/**
 * @brief Control the format string for a given progress bar widget's units
 * label
 *
 * If @c NULL is passed on @c format, it will make @c obj's units area to be
 * hidden completely. If not, it'll set the <b>format string</b> for the units
 * label's text. The units label is provided a floating point value, so the
 * units text is up display at most one floating point value. Note that the
 * units label is optional. Use a format string such as "%1.2f meters" for
 * example.
 *
 * @note The default format string for a progress bar is an integer percentage,
 * as in $"%.0f %%".
 *
 * @param[in] units The format string for @c obj's units label
 *
 * @ingroup Elm_Progressbar
 */
EAPI void elm_progressbar_unit_format_set(Evas_Object *obj, const char *units);

/**
 * @brief Control the format string for a given progress bar widget's units
 * label
 *
 * If @c NULL is passed on @c format, it will make @c obj's units area to be
 * hidden completely. If not, it'll set the <b>format string</b> for the units
 * label's text. The units label is provided a floating point value, so the
 * units text is up display at most one floating point value. Note that the
 * units label is optional. Use a format string such as "%1.2f meters" for
 * example.
 *
 * @note The default format string for a progress bar is an integer percentage,
 * as in $"%.0f %%".
 *
 * @return The format string for @c obj's units label
 *
 * @ingroup Elm_Progressbar
 */
EAPI const char *elm_progressbar_unit_format_get(const Evas_Object *obj);

/**
 * @brief Set the format function pointer for the units label
 *
 * Set the callback function to format the unit string.
 *
 * See: @ref elm_progressbar_unit_format_set for more info on how this works.
 *
 * @param[in] func The unit format function
 * @param[in] free_func The freeing function for the format string.
 *
 * @since 1.7
 *
 * @ingroup Elm_Progressbar
 */
EAPI void elm_progressbar_unit_format_function_set(Evas_Object *obj, progressbar_func_type func, progressbar_freefunc_type free_func);

#include "elm_progressbar.eo.legacy.h"