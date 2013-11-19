#ifdef ENABLE_HOLY_BIBLE
#ifndef __Z03_BOT_ACTIONS_BIBLE_H
        #define __Z03_BOT_ACTIONS_BIBLE_H
        
        void action_bible(ircmessage_t *message, char *args);

        typedef struct verse_t {
                int book_id;
                char *book_title;
                int chapter;
                int verse;
                char *text;
                
        } verse_t;
        
        #define BIBLE_DATABASE_FILE       "holy-bible.sqlite3"
        
#endif // __Z03_BOT_ACTIONS_BIBLE_H
#endif // ENABLE_HOLY_BIBLE
