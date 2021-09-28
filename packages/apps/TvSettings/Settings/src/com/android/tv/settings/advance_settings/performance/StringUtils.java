package com.android.tv.settings.advance_settings.performance;

public class StringUtils {

        /**
         * Check that the given CharSequence is neither <code>null</code> nor of length 0.
         * @return <code>true</code> if the obj is not null and obj.toString() has length
         */
        public static boolean isEmptyObj(Object obj) {
              return obj == null || "".equals(obj);
        }

        /**
         * Check that the given CharSequence is neither <code>null</code> nor of length 0.
         * Note: Will return <code>true</code> for a CharSequence that purely consists of whitespace.
         * <p><pre>
         * StringUtils.hasLength(null) = false
         * StringUtils.hasLength("") = false
         * StringUtils.hasLength(" ") = true
         * StringUtils.hasLength("Hello") = true
         * </pre>
         * @param str the CharSequence to check (may be <code>null</code>)
         * @return <code>true</code> if the CharSequence is not null and has length
         * @see #hasText(String)
         */
         public static boolean hasLength(CharSequence str) {
                return (str != null && str.length() > 0);
         }

        /**
         * Convert string to int
         * @param str
         * @param def
         * @return
         */
         public static int parseInt(String str, int def){
                int result = def;
                try{
                    if(str!=null)
                       result = Integer.parseInt(str);
                }catch(Exception e){
                }
                return result;
         }
}
