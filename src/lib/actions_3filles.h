#ifdef ENABLE_3FILLES_BOOK
#ifndef __Z03_BOT_ACTIONS_3FILLES_H
        #define __Z03_BOT_ACTIONS_3FILLES_H
        
        void action_3filles(ircmessage_t *message, char *args);

        typedef struct book_t {
                int chapter;
                int line;
                char *text;
                
        } book_t;
        
        #define TROISFILLES_DATABASE_FILE       "3filles.sqlite3"
        
#endif // __Z03_BOT_ACTIONS_3FILLES_H
#endif // ENABLE_3FILLES_BOOK
