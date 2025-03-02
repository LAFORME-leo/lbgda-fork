/*
 * Copyright (C) 2008 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "GdaJBlobOp.h"
#include "jni-wrapper.h"
#include "jni-globals.h"
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "gda-jdbc-blob-op.h"


static JniWrapperMethod *GdaJValue__createDate = NULL;
static JniWrapperMethod *GdaJValue__createTime = NULL;
static JniWrapperMethod *GdaJValue__createTimestamp = NULL;

JNIEXPORT void
JNICALL Java_GdaJValue_initIDs (JNIEnv *env, jclass klass)
{
	gsize i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"createDate", "(III)Ljava/sql/Date;", TRUE, &GdaJValue__createDate},
		{"createTime", "(III)Ljava/sql/Time;", TRUE, &GdaJValue__createTime},
		{"createTimestamp", "(IIIIII)Ljava/sql/Timestamp;", TRUE, &GdaJValue__createTimestamp},
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJValue", m->name);
	}
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCString (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer, jint col, jstring str)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	gchar *tmp;
	gint len, ulen;

	len = (*jenv)->GetStringUTFLength (jenv, str);
	if ((*jenv)->ExceptionCheck (jenv))
		return;
	ulen = (*jenv)->GetStringLength (jenv, str);
	if ((*jenv)->ExceptionCheck (jenv))
		return;
	tmp = g_new (gchar, len + 1);
	tmp [len] = 0;
	if (ulen > 0)
		(*jenv)->GetStringUTFRegion (jenv, str, 0, ulen, tmp);
	if ((*jenv)->ExceptionCheck (jenv)) {
		g_free (tmp);
		return;
	}
	g_value_init (value, G_TYPE_STRING);
	g_value_take_string (value, tmp);
}

JNIEXPORT jstring
JNICALL Java_GdaJValue_getCString (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	const gchar *str;
	
	str = g_value_get_string ((GValue *) c_pointer);
	return (*jenv)->NewStringUTF (jenv, str);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCInt (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				jint col, jint i)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, i);
}

JNIEXPORT jint
JNICALL Java_GdaJValue_getCInt (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jint) g_value_get_int ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCChar (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				 jint col, jbyte b)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_CHAR);
	g_value_set_char (value, b);
}

JNIEXPORT jbyte
JNICALL Java_GdaJValue_getCChar (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jbyte) g_value_get_char ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCDouble (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				   jint col, jdouble d)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, d);
}

JNIEXPORT jdouble
JNICALL Java_GdaJValue_getCDouble (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jdouble) g_value_get_double ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCFloat (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				  jint col, jfloat f)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_FLOAT);
	g_value_set_float (value, f);
}

JNIEXPORT jfloat
JNICALL Java_GdaJValue_getCFloat (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jfloat) g_value_get_float ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCBoolean (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				    jint col, jboolean b)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, b);
}

JNIEXPORT jboolean
JNICALL Java_GdaJValue_getCBoolean (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jboolean) g_value_get_boolean ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCDate (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer, jint col,
				 jint year, jint month, jint day)
{
	GDate *date;

	date = g_date_new_dmy (day, month, year);
	if (g_date_valid (date)) {
		GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
		g_value_init (value, G_TYPE_DATE);
		g_value_take_boxed (value, date);
	}
	else {
		g_date_free (date);
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return;
		}
		gchar *tmp = g_strdup_printf (_("Invalid date: year %d, month %d and day %d"), year, month, day);
		(*jenv)->ThrowNew (jenv, cls, tmp);
		g_free (tmp);
	}
}

JNIEXPORT jobject JNICALL
Java_GdaJValue_getCDate (JNIEnv *jenv, jobject obj, jlong c_pointer)
{
	const GDate *date;
	jobject jobj;

	date = g_value_get_boxed ((GValue *) c_pointer);
	if (!date || ! g_date_valid (date)) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	jobj = (*jenv)->CallObjectMethod (jenv, obj, GdaJValue__createDate->mid, 
					  g_date_get_year (date), g_date_get_month (date) - 1, g_date_get_day (date));
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;
	else
		return jobj;
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCTime (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				 jint col, jint hour, jint min, jint sec)
{
	GdaTime *tim;
	GValue *value;

	tim = g_new0 (GdaTime, 1);
	tim->hour = hour;
	tim->minute = min;
	tim->second = sec;

	value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, GDA_TYPE_TIME);
	g_value_take_boxed (value, tim);
}

JNIEXPORT jobject
JNICALL Java_GdaJValue_getCTime (JNIEnv *jenv, jobject obj, jlong c_pointer)
{
	const GdaTime *tim;
	jobject jobj;

	tim = g_value_get_boxed ((GValue *) c_pointer);
	if (!tim) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	jobj = (*jenv)->CallObjectMethod (jenv, obj, GdaJValue__createTime->mid, 
					  tim->hour, tim->minute, tim->second);
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;
	else
		return jobj;
}


JNIEXPORT void
JNICALL Java_GdaJValue_setCTimestamp (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				      jint col, jint year, jint month, jint day, jint hour, jint min,
				      jint sec)
{
	GdaTimestamp *ts;
	GValue *value;

	ts = g_new0 (GdaTimestamp, 1);
	ts->year = year;
	ts->month = month;
	ts->day = day;
	ts->hour = hour;
	ts->minute = min;
	ts->second = sec;

	value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, GDA_TYPE_TIMESTAMP);
	g_value_take_boxed (value, ts);
}

JNIEXPORT jobject
JNICALL Java_GdaJValue_getCTimestamp (JNIEnv *jenv, jobject obj, jlong c_pointer)
{
	const GdaTimestamp *ts;
	jobject jobj;

	ts = g_value_get_boxed ((GValue *) c_pointer);
	if (!ts) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	jobj = (*jenv)->CallObjectMethod (jenv, obj, GdaJValue__createTimestamp->mid, 
					  ts->year, ts->month, ts->day,
					  ts->hour, ts->minute, ts->second);
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;
	else
		return jobj;
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCBinary (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer, jint col,
				   jbyteArray bytes)
{
	jint len;
	GdaBinary *bin;
	GValue *value;
	
	len = (*jenv)->GetArrayLength (jenv, bytes);
	
	bin = g_new0 (GdaBinary, 1);
	bin->binary_length = len;
	bin->data = g_new (guchar, len);
	(*jenv)->GetByteArrayRegion(jenv, bytes, 0, len, (jbyte *) bin->data);

	value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, GDA_TYPE_BINARY);
	g_value_take_boxed (value, bin);
}

JNIEXPORT jbyteArray
JNICALL Java_GdaJValue_getCBinary (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	const GdaBinary *bin;
	jbyteArray jbytes;

	bin = g_value_get_boxed ((GValue *) c_pointer);
	if (!bin) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	jbytes = (*jenv)->NewByteArray (jenv, bin->binary_length);
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;

	(*jenv)->SetByteArrayRegion (jenv, jbytes, 0, bin->binary_length, (jbyte*) bin->data);
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;

	return jbytes;
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCBlob (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer, jint col,
				 jlong cnc_c_pointer, jobject blobop)
{
	GdaBlob *blob;
	GValue *value;

	blob = g_new0 (GdaBlob, 1);
	blob->op = gda_jdbc_blob_op_new_with_jblob (GDA_CONNECTION ((gpointer) cnc_c_pointer), jenv, blobop);

	value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_take_boxed (value, blob);
}

JNIEXPORT jobject
JNICALL Java_GdaJValue_getCBlob (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	const GdaBlob *blob;

	blob = g_value_get_boxed ((GValue *) c_pointer);
	if (!blob) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	/* create a GdaInputStream */
	if (blob->op) {
		jobject retobj;
		jmethodID mid;
		mid = (*jenv)->GetMethodID (jenv, GdaInputStream__class, "<init>", "(JJ)V");
		if ((*jenv)->ExceptionCheck (jenv))
			return NULL;

		glong size = gda_blob_op_get_length (blob->op);
		if (size < 0) {
			/* throw an exception */
			jclass cls;
			cls = (*jenv)->FindClass (jenv, "java/sql/SQLException");
			if (!cls) {
				/* Unable to find the exception class */
				return NULL;
			}
			(*jenv)->ThrowNew (jenv, cls, _("Can't get BLOB's size"));
			return NULL;
		}
		retobj = (*jenv)->NewObject (jenv, GdaInputStream__class, mid, (jlong) blob,
					     (jlong) size);
		if ((*jenv)->ExceptionCheck (jenv))
			return NULL;
		return retobj;
	}
	else {
		jobject retobj;
		jmethodID mid;
		jbyteArray jbytes;
		GdaBinary *bin = (GdaBinary *) blob;

		mid = (*jenv)->GetMethodID (jenv, GdaInputStream__class, "<init>", "([B)V");
		if ((*jenv)->ExceptionCheck (jenv))
			return NULL;

		jbytes = (*jenv)->NewByteArray (jenv, bin->binary_length);
		if ((*jenv)->ExceptionCheck (jenv))
			return NULL;
		
		(*jenv)->SetByteArrayRegion (jenv, jbytes, 0, bin->binary_length, (jbyte *) bin->data);
		if ((*jenv)->ExceptionCheck (jenv))
			return NULL;
		
		retobj = (*jenv)->NewObject (jenv, GdaInputStream__class, mid, jbytes);
		if ((*jenv)->ExceptionCheck (jenv))
			return NULL;
		return retobj;
	}
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCLong (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer, jint col, jlong l)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_INT64);
	g_value_set_int64 (value, l);
}

JNIEXPORT jshort
JNICALL Java_GdaJValue_getCLong (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jlong) g_value_get_int64 ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCShort (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer,
				  jint col, jshort s)
{
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	g_value_init (value, G_TYPE_INT64);
	gda_value_set_short (value, s);
}

JNIEXPORT jshort
JNICALL Java_GdaJValue_getCShort (G_GNUC_UNUSED JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	return (jshort) gda_value_get_short ((GValue *) c_pointer);
}

JNIEXPORT void
JNICALL Java_GdaJValue_setCNumeric (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer, jint col,
				    jstring str, jint precision, jint scale)
{
	GdaNumeric *num;
	GValue *value = gda_row_get_value (GDA_ROW ((gpointer) c_pointer), col);
	gchar *tmp;
	gint len, ulen;

	len = (*jenv)->GetStringUTFLength (jenv, str);
	if ((*jenv)->ExceptionCheck (jenv))
		return;
	ulen = (*jenv)->GetStringLength (jenv, str);
	if ((*jenv)->ExceptionCheck (jenv))
		return;
	tmp = g_new (gchar, len + 1);
	tmp [len] = 0;
	if (ulen > 0)
		(*jenv)->GetStringUTFRegion (jenv, str, 0, ulen, tmp);
	if ((*jenv)->ExceptionCheck (jenv)) {
		g_free (tmp);
		return;
	}
	num = g_new0 (GdaNumeric, 1);
	num->number = tmp;
	num->precision = precision;
	num->width = scale;
	g_value_init (value, GDA_TYPE_NUMERIC);
	g_value_take_boxed (value, num);
}

JNIEXPORT jobject
JNICALL Java_GdaJValue_getCNumeric (JNIEnv *jenv, G_GNUC_UNUSED jobject obj, jlong c_pointer)
{
	const GdaNumeric *num;
	num = gda_value_get_numeric ((GValue *) c_pointer);
	if (!num) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	jobject retobj;
	jmethodID mid;
	jstring string;
	jclass cls;

	cls = (*jenv)->FindClass (jenv, "java/math/BigDecimal");
	if (!cls) {
		/* Unable to find the BigDecimal class */
		return NULL;
	}
	
	mid = (*jenv)->GetStaticMethodID (jenv, cls, "<init>", "(Ljava/lang/String;)V");
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;

	string = (*jenv)->NewStringUTF (jenv, num->number);
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;

	retobj = (*jenv)->NewObject (jenv, GdaInputStream__class, mid, string);
	if ((*jenv)->ExceptionCheck (jenv))
		return NULL;
	return retobj;
}
