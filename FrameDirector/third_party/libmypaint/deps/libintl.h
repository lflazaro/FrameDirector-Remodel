#ifndef LIBINTL_H
#define LIBINTL_H

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *gettext(const char *msgid)
{
    return msgid;
}

static inline const char *dgettext(const char *domainname, const char *msgid)
{
    (void)domainname;
    return msgid;
}

static inline const char *dcgettext(const char *domainname, const char *msgid, int category)
{
    (void)domainname;
    (void)category;
    return msgid;
}

static inline const char *bindtextdomain(const char *domainname, const char *dirname)
{
    (void)dirname;
    return domainname;
}

static inline const char *textdomain(const char *domainname)
{
    return domainname;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBINTL_H */
