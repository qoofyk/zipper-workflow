#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# referenced python binding in spark k8s
ARG base_img
FROM $base_img
WORKDIR /

# Reset to root to run installation tasks
USER 0

RUN mkdir ${SPARK_HOME}/python
# TODO: Investigate running both pip and pip3 via virtualenvs
RUN apt install -y python python-pip && \
    apt install -y python3 python3-pip && \
    # We remove ensurepip since it adds no functionality since pip is
    # installed on the image and it just takes up 1.6MB on the image
    rm -r /usr/lib/python*/ensurepip && \
    pip install --upgrade pip setuptools && \
    # You may install with python3 packages by using pip3.6
    # Removed the .cache to save space
    rm -r /root/.cache && rm -rf /var/cache/apt/*

COPY python/lib ${SPARK_HOME}/python/lib
#ENV PYTHONPATH ${SPARK_HOME}/python/lib/pyspark.zip:${SPARK_HOME}/python/lib/py4j-*.zip


ARG workdir=/opt/spark/work-dir
WORKDIR $workdir

# Only ship spark-redis as deps
RUN mkdir ${workdir}/deps
COPY dmd-deps/*  ${workdir}/deps/
ENV SPARK_EXTRA_CLASSPATH  ${workdir}/deps/spark-redis_2.11-2.4.3-SNAPSHOT-jar-with-dependencies.jar

RUN pip3 install -r ${workdir}/deps/requirements.txt && \
  rm -r /root/.cache
#ENV PYTHONPATH ${SPARK_HOME}/python/lib/pyspark.zip:${SPARK_HOME}/python/lib/py4j-*.zip:${workdir}/deps/fluiddmd-0.1-py3.6.egg
#ENV PYSPARK_FILES  ${workdir}/deps/fluiddmd-0.1-py3.6.egg

# provide egg. pyfile, and jar file through http 

ENTRYPOINT [ "/opt/entrypoint.sh" ]

# Specify the User that the actual main process will run as
ARG spark_uid=185
USER ${spark_uid}
