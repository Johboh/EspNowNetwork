FROM python:3.8.16-alpine3.16

RUN pip install flask

ADD ./main.py /
ADD ./entrypoint.sh /

RUN chmod +x *.py
RUN chmod +x *.sh

CMD [ "/entrypoint.sh" ]
