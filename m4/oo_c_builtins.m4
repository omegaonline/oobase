
AC_DEFUN([OO_C_BUILTINS],
[
	AC_LANG_PUSH([C++])
	AC_MSG_CHECKING([for __sync_val_compare_and_swap compiler intrinsic])
	AC_COMPILE_IFELSE(
        [
			AC_LANG_PROGRAM([[ ]],
            [[
				long x = 12;
				long y = __sync_val_compare_and_swap(&x,57,60);
				++y;
            ]]
        )
        ],
        [
			AC_MSG_RESULT([yes])
			AC_DEFINE([HAVE___SYNC_VAL_COMPARE_AND_SWAP], [1], [Define to 1 if you have the __sync_val_compare_and_swap compiler intrinsic])
        ],
        [
			AC_MSG_RESULT([unsupported])
        ]
    )
    
    AC_MSG_CHECKING([for __builtin_bswap16 compiler intrinsic])
	AC_COMPILE_IFELSE(
        [
			AC_LANG_PROGRAM([[ ]],
            [[
				short x = 12;
				short y = __builtin_bswap16(x);
				++y;
            ]]
        )
        ],
        [
			AC_MSG_RESULT([yes])
			AC_DEFINE([HAVE___BUILTIN_BSWAP16], [1], [Define to 1 if you have the __builtin_bswap16 compiler intrinsic])
        ],
        [
			AC_MSG_RESULT([unsupported])
        ]
    )
    
    AC_MSG_CHECKING([for __builtin_bswap32 compiler intrinsic])
	AC_COMPILE_IFELSE(
        [
			AC_LANG_PROGRAM([[ ]],
            [[
				long x = 12;
				long y = __builtin_bswap32(x);
				++y;
            ]]
        )
        ],
        [
			AC_MSG_RESULT([yes])
			AC_DEFINE([HAVE___BUILTIN_BSWAP32], [1], [Define to 1 if you have the __builtin_bswap32 compiler intrinsic])
        ],
        [
			AC_MSG_RESULT([unsupported])
        ]
    )
    
    AC_MSG_CHECKING([for __builtin_bswap64 compiler intrinsic])
	AC_COMPILE_IFELSE(
        [
			AC_LANG_PROGRAM([[ ]],
            [[
				long long x = 12;
				long long y = __builtin_bswap64(x);
				++y;
            ]]
        )
        ],
        [
			AC_MSG_RESULT([yes])
			AC_DEFINE([HAVE___BUILTIN_BSWAP64], [1], [Define to 1 if you have the __builtin_bswap64 compiler intrinsic])
        ],
        [
			AC_MSG_RESULT([unsupported])
        ]
    )
    
    AC_MSG_CHECKING([for __builtin_ffs compiler intrinsic])
	AC_COMPILE_IFELSE(
        [
			AC_LANG_PROGRAM([[ ]],
            [[
				unsigned int x = 12;
				unsigned int y = __builtin_ffs(x);
				++y;
            ]]
        )
        ],
        [
			AC_MSG_RESULT([yes])
			AC_DEFINE([HAVE___BUILTIN_FFS], [1], [Define to 1 if you have the __builtin_ffs compiler intrinsic])
        ],
        [
			AC_MSG_RESULT([unsupported])
        ]
    )
    
    AC_LANG_POP([C++])
])
