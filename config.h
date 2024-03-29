/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int statusrpad = 12;     /* padding right of status */
static const unsigned int borderpx = 1;        /* border pixel of windows */
static const unsigned int gappx    = 10;       /* gaps between windows */
static const unsigned int snap     = 0;        /* snap pixel */
static const int showbar           = 1;        /* 0 means no bar */
static const int topbar            = 1;        /* 0 means bottom bar */
static const char *fonts[]         = { 
	"DejaVuSansM Nerd Font:pixelsize=12:antialias=true:autohint=true"
	"DejaVu Sans Mono Nerd Font:pixelsize=12:antialias=true:autohint=true",
};
static const char col_gray1[]      = "#222222";
static const char col_gray2[]      = "#444444";
static const char col_gray3[]      = "#bbbbbb";
static const char col_gray4[]      = "#eeeeee";
static const char col_red[]        = "#f90f47";
static const char col_white[]      = "#ffffff";
static const float barpady         = 12; /* y-padding of bar; if ≤ 1, padding is rel. to font height, otherwise absolute in pixels */
static const unsigned int baralpha = OPAQUE;
static const unsigned int borderalpha = OPAQUE;
static const char *colors[][3]     = {
	/*               fg       , bg       , border */
	[SchemeNorm] = { col_white, col_gray1, col_gray1 },
	[SchemeSel]  = { col_gray4, col_red  , col_red   },
};
static const unsigned int alphas[][3] = {
  [SchemeNorm] = { OPAQUE, baralpha, borderalpha },
  [SchemeSel] = { OPAQUE, baralpha, borderalpha },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *  WM_CLASS(STRING) -> instance, class
	 *  WM_NAME(STRING)  -> title
	 */
	/* class            , instance , title                          , tags mask , isfloating , isprotected , monitor */
	{ "Gimp"            , NULL     , NULL                           , 0         , 1          , 0           , -1 } ,
	{ "Microsoft Teams" , NULL     , "Microsoft Teams Notification" , 0         , 1          , 0           , -1 } ,
 };

/* window swallowing */
static const int swaldecay = 3;
static const int swalretroactive = 1;
static const char swalsymbol[] = "👅";

/* window protection */
static const char protectedsymbol[] = "🔒";

/* fake fullscreen */
static const char fakefullscreensymbol[] = "⛶";

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals. Set to 0 to remove gaps between terminals. */

static const Layout layouts[] = {
	/* symbol, arrange function */
	{ "◧"    , tile    }, /* first entry is default */
	{ "~"    , NULL    }, /* no layout function means floating behavior */
	{ "o"    , monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY                      , KEY, view      , {.ui = 1 << TAG}}, \
	{ MODKEY|ControlMask          , KEY, toggleview, {.ui = 1 << TAG}}, \
	{ MODKEY|ShiftMask            , KEY, tag       , {.ui = 1 << TAG}}, \
	{ MODKEY|ControlMask|ShiftMask, KEY, toggletag , {.ui = 1 << TAG}},

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static Key keys[] = {
	/* modifier                   , key      , function      , argument            , */
	{ MODKEY                       , XK_b      , togglebar       , {0} }                , // toggle bar
	{ MODKEY                       , XK_j      , focusstack      , {.i = +1 } }         , // navigate stack
	{ MODKEY                       , XK_k      , focusstack      , {.i = -1 } }         , // navigate stack
	{ MODKEY|ShiftMask             , XK_j      , moveclient      , {.i = +1 } }         , // navigate client within stack
	{ MODKEY|ShiftMask             , XK_k      , moveclient      , {.i = -1 } }         , // navigate client within stack
	{ MODKEY                       , XK_i      , incnmaster      , {.i = +1 } }         , // inc num of masters
	{ MODKEY|ShiftMask             , XK_i      , incnmaster      , {.i = -1 } }         , // dec num of masters
	{ MODKEY|ShiftMask             , XK_comma  , setmfact        , {.f = -0.05} }       , // dec master area
	{ MODKEY|ShiftMask             , XK_period , setmfact        , {.f = +0.05} }       , // inc master area
	{ MODKEY|ShiftMask             , XK_Return , zoom            , {0} }                , // raise to top of stack
	{ MODKEY                       , XK_u      , swalstopsel     , {0} }                ,
	{ MODKEY                       , XK_Tab    , view            , {0} }                , // select previous tag
	{ MODKEY                       , XK_q      , killclient      , {0} }                ,
	{ MODKEY                       , XK_minus  , setcfact        , {.f = -0.25} }       ,
	{ MODKEY|ShiftMask             , XK_equal  , setcfact        , {.f = +0.25} }       ,
	{ MODKEY                       , XK_t      , setlayout       , {.v = &layouts[0]} } ,
	{ MODKEY                       , XK_f      , togglecovertfullscreen, {0} } ,
	{ MODKEY                       , XK_space  , setlayout       , {0} }                ,
	{ MODKEY|ShiftMask             , XK_t      , toggleprotected , {0} }                ,
	{ MODKEY|ShiftMask             , XK_space  , togglefloating  , {0} }                , // toggle float for client
	{ MODKEY                       , XK_0      , view            , {.ui = ~0 } }        , // view all tags
	{ MODKEY|ShiftMask             , XK_0      , tag             , {.ui = ~0 } }        , // tag client with all tags
	{ MODKEY                       , XK_l      , focusmon        , {.i = +1 } }         ,
	{ MODKEY                       , XK_h      , focusmon        , {.i = -1 } }         ,
	{ MODKEY|ShiftMask             , XK_l      , tagmon          , {.i = +1 } }         ,
	{ MODKEY|ShiftMask             , XK_h      , tagmon          , {.i = -1 } }         ,
	{ MODKEY|ControlMask|ShiftMask , XK_equal  , setgaps         , {.i = +1 } }         ,
	{ MODKEY|ControlMask           , XK_minus  , setgaps         , {.i = -1 } }         ,
	{ MODKEY|ShiftMask             , XK_r      , quit            , {1}              }   , // reload dwm in-place
	TAGKEYS(XK_1, 0)
	TAGKEYS(XK_2, 1)
	TAGKEYS(XK_3, 2)
	TAGKEYS(XK_4, 3)
	TAGKEYS(XK_5, 4)
	TAGKEYS(XK_6, 5)
	TAGKEYS(XK_7, 6)
	TAGKEYS(XK_8, 7)
	TAGKEYS(XK_9, 8)
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click        , event mask       , button  , function       , argument */
	{ ClkLtSymbol   , 0                , Button1 , setlayout      , {0} }                ,
	{ ClkLtSymbol   , 0                , Button3 , setlayout      , {.v = &layouts[2]} } ,
	{ ClkWinTitle   , 0                , Button2 , zoom           , {0} }                ,
	{ ClkClientWin  , MODKEY           , Button1 , movemouse      , {0} }                ,
	{ ClkClientWin  , MODKEY           , Button2 , togglefloating , {0} }                ,
	{ ClkClientWin  , MODKEY           , Button3 , resizemouse    , {0} }                ,
	{ ClkClientWin  , MODKEY|ShiftMask , Button1 , swalmouse      , {0} }                ,
	{ ClkTagBar     , 0                , Button1 , view           , {0} }                ,
	{ ClkTagBar     , 0                , Button3 , toggleview     , {0} }                ,
	{ ClkTagBar     , MODKEY           , Button1 , tag            , {0} }                ,
	{ ClkTagBar     , MODKEY           , Button3 , toggletag      , {0} }                ,
	{ ClkRootWin    , 0                , Button1 , spawn          , SHCMD("st") }        ,
};
