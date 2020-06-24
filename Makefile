# ############################################################################ #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: charles <charles.cabergs@gmail.com>        +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2020/06/24 08:06:38 by charles           #+#    #+#              #
#    Updated: 2020/06/24 08:11:12 by charles          ###   ########.fr        #
#                                                                              #
# ############################################################################ #


RM = rm -f

INCDIR = inc
SRCDIR = src
OBJDIR = obj
OBJDIRS = $(shell find $(SRCDIR) -type d | sed 's/src/$(OBJDIR)/')

INC = $(shell find $(INCDIR) -name "*.h")

SRC = $(shell find $(SRCDIR) -name "*.c")

OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

CC = gcc
CCFLAGS = -g -I$(INCDIR) -Wall -Wextra #-Werror

NAME = ft_ping

all: prebuild $(NAME)

prebuild:
	@mkdir -vp $(OBJDIR)
	@for subdir in $(OBJDIRS); do mkdir -vp $$subdir; done

$(NAME): $(OBJ)
	@echo "Linking: $@"
	@$(CC) -o $@ $(OBJ)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INC)
	@echo "Compiling: $@"
	@$(CC) $(CCFLAGS) -c -o $@ $<

clean:
	@echo "Removing objects"
	@$(RM) -r $(OBJDIR)

fclean:
	@echo "Removing objects"
	@$(RM) -r $(OBJDIR)
	@echo "Removing exectable"
	@$(RM) $(NAME)

re: fclean all

.PHONY: all prebuild clean fclean re
